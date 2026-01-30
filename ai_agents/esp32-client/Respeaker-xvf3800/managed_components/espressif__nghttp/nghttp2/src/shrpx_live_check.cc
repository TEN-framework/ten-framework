/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_live_check.h"
#include "shrpx_worker.h"
#include "shrpx_connect_blocker.h"
#include "shrpx_tls.h"
#include "shrpx_log.h"

namespace shrpx {

namespace {
constexpr size_t MAX_BUFFER_SIZE = 4_k;
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto conn = static_cast<Connection *>(w->data);
  auto live_check = static_cast<LiveCheck *>(conn->data);

  rv = live_check->do_read();
  if (rv != 0) {
    live_check->on_failure();
    return;
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto conn = static_cast<Connection *>(w->data);
  auto live_check = static_cast<LiveCheck *>(conn->data);

  rv = live_check->do_write();
  if (rv != 0) {
    live_check->on_failure();
    return;
  }
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto live_check = static_cast<LiveCheck *>(conn->data);

  if (w == &conn->rt && !conn->expired_rt()) {
    return;
  }

  live_check->on_failure();
}
} // namespace

namespace {
void backoff_timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  int rv;
  auto live_check = static_cast<LiveCheck *>(w->data);

  rv = live_check->initiate_connection();
  if (rv != 0) {
    live_check->on_failure();
    return;
  }
}
} // namespace

namespace {
void settings_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto live_check = static_cast<LiveCheck *>(w->data);

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "SETTINGS timeout";
  }

  live_check->on_failure();
}
} // namespace

LiveCheck::LiveCheck(struct ev_loop *loop, SSL_CTX *ssl_ctx, Worker *worker,
                     DownstreamAddr *addr, std::mt19937 &gen)
  : conn_(loop, -1, nullptr, worker->get_mcpool(),
          worker->get_downstream_config()->timeout.write,
          worker->get_downstream_config()->timeout.read, {}, {}, writecb,
          readcb, timeoutcb, this, get_config()->tls.dyn_rec.warmup_threshold,
          get_config()->tls.dyn_rec.idle_timeout, Proto::NONE),
    wb_(worker->get_mcpool()),
    gen_(gen),
    read_(&LiveCheck::noop),
    write_(&LiveCheck::noop),
    worker_(worker),
    ssl_ctx_(ssl_ctx),
    addr_(addr),
    session_(nullptr),
    raddr_(nullptr),
    success_count_(0),
    fail_count_(0),
    settings_ack_received_(false),
    session_closing_(false) {
  ev_timer_init(&backoff_timer_, backoff_timeoutcb, 0., 0.);
  backoff_timer_.data = this;

  // SETTINGS ACK must be received in a short timeout.  Otherwise, we
  // assume that connection is broken.
  ev_timer_init(&settings_timer_, settings_timeout_cb, 0., 0.);
  settings_timer_.data = this;
}

LiveCheck::~LiveCheck() {
  disconnect();

  ev_timer_stop(conn_.loop, &backoff_timer_);
}

void LiveCheck::disconnect() {
  if (dns_query_) {
    auto dns_tracker = worker_->get_dns_tracker();

    dns_tracker->cancel(dns_query_.get());
  }

  dns_query_.reset();
  // We can reuse resolved_addr_
  raddr_ = nullptr;

  conn_.rlimit.stopw();
  conn_.wlimit.stopw();

  ev_timer_stop(conn_.loop, &settings_timer_);

  read_ = write_ = &LiveCheck::noop;

  conn_.disconnect();

  nghttp2_session_del(session_);
  session_ = nullptr;

  settings_ack_received_ = false;
  session_closing_ = false;

  wb_.reset();
}

// Use the similar backoff algorithm described in
// https://github.com/grpc/grpc/blob/master/doc/connection-backoff.md
namespace {
constexpr size_t MAX_BACKOFF_EXP = 10;
constexpr auto MULTIPLIER = 1.6;
constexpr auto JITTER = 0.2;
} // namespace

void LiveCheck::schedule() {
  auto base_backoff =
    util::int_pow(MULTIPLIER, std::min(fail_count_, MAX_BACKOFF_EXP));
  auto dist = std::uniform_real_distribution<>(-JITTER * base_backoff,
                                               JITTER * base_backoff);

  auto &downstreamconf = *get_config()->conn.downstream;

  auto backoff =
    std::min(downstreamconf.timeout.max_backoff, base_backoff + dist(gen_));

  ev_timer_set(&backoff_timer_, backoff, 0.);
  ev_timer_start(conn_.loop, &backoff_timer_);
}

int LiveCheck::do_read() { return read_(*this); }

int LiveCheck::do_write() { return write_(*this); }

int LiveCheck::initiate_connection() {
  int rv;

  auto worker_blocker = worker_->get_connect_blocker();
  if (worker_blocker->blocked()) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Worker wide backend connection was blocked temporarily";
    }
    return -1;
  }

  if (!dns_query_ && addr_->tls) {
    assert(ssl_ctx_);

    auto ssl = tls::create_ssl(ssl_ctx_);
    if (!ssl) {
      return -1;
    }

    switch (addr_->proto) {
    case Proto::HTTP1:
      tls::setup_downstream_http1_alpn(ssl);
      break;
    case Proto::HTTP2:
      tls::setup_downstream_http2_alpn(ssl);
      break;
    default:
      assert(0);
    }

    conn_.set_ssl(ssl);
    conn_.tls.client_session_cache = &addr_->tls_session_cache;
  }

  if (addr_->dns) {
    if (!dns_query_) {
      auto dns_query = std::make_unique<DNSQuery>(
        addr_->host, [this](DNSResolverStatus status, const Address *result) {
          int rv;

          if (status == DNSResolverStatus::OK) {
            *this->resolved_addr_ = *result;
          }
          rv = this->initiate_connection();
          if (rv != 0) {
            this->on_failure();
          }
        });
      auto dns_tracker = worker_->get_dns_tracker();

      if (!resolved_addr_) {
        resolved_addr_ = std::make_unique<Address>();
      }

      switch (dns_tracker->resolve(resolved_addr_.get(), dns_query.get())) {
      case DNSResolverStatus::ERROR:
        return -1;
      case DNSResolverStatus::RUNNING:
        dns_query_ = std::move(dns_query);
        return 0;
      case DNSResolverStatus::OK:
        break;
      default:
        assert(0);
      }
    } else {
      switch (dns_query_->status) {
      case DNSResolverStatus::ERROR:
        dns_query_.reset();
        return -1;
      case DNSResolverStatus::OK:
        dns_query_.reset();
        break;
      default:
        assert(0);
      }
    }

    util::set_port(*resolved_addr_, addr_->port);
    raddr_ = resolved_addr_.get();
  } else {
    raddr_ = &addr_->addr;
  }

  conn_.fd = util::create_nonblock_socket(raddr_->su.storage.ss_family);

  if (conn_.fd == -1) {
    auto error = errno;
    LOG(WARN) << "socket() failed; addr=" << util::to_numeric_addr(raddr_)
              << ", errno=" << error;
    return -1;
  }

  rv = connect(conn_.fd, &raddr_->su.sa, raddr_->len);
  if (rv != 0 && errno != EINPROGRESS) {
    auto error = errno;
    LOG(WARN) << "connect() failed; addr=" << util::to_numeric_addr(raddr_)
              << ", errno=" << error;

    close(conn_.fd);
    conn_.fd = -1;

    return -1;
  }

  if (addr_->tls) {
    auto sni_name =
      addr_->sni.empty() ? StringRef{addr_->host} : StringRef{addr_->sni};
    if (!util::numeric_host(sni_name.data())) {
      SSL_set_tlsext_host_name(conn_.tls.ssl, sni_name.data());
    }

    auto session = tls::reuse_tls_session(addr_->tls_session_cache);
    if (session) {
      SSL_set_session(conn_.tls.ssl, session);
      SSL_SESSION_free(session);
    }

    conn_.prepare_client_handshake();
  }

  write_ = &LiveCheck::connected;

  ev_io_set(&conn_.wev, conn_.fd, EV_WRITE);
  ev_io_set(&conn_.rev, conn_.fd, EV_READ);

  conn_.wlimit.startw();

  auto &downstreamconf = *get_config()->conn.downstream;

  conn_.wt.repeat = downstreamconf.timeout.connect;
  ev_timer_again(conn_.loop, &conn_.wt);

  return 0;
}

int LiveCheck::connected() {
  auto sock_error = util::get_socket_error(conn_.fd);
  if (sock_error != 0) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Backend connect failed; addr="
                << util::to_numeric_addr(raddr_) << ": errno=" << sock_error;
    }

    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Connection established";
  }

  auto &downstreamconf = *get_config()->conn.downstream;

  // Reset timeout for write.  Previously, we set timeout for connect.
  conn_.wt.repeat = downstreamconf.timeout.write;
  ev_timer_again(conn_.loop, &conn_.wt);

  conn_.rlimit.startw();
  conn_.again_rt();

  if (conn_.tls.ssl) {
    read_ = &LiveCheck::tls_handshake;
    write_ = &LiveCheck::tls_handshake;

    return do_write();
  }

  if (addr_->proto == Proto::HTTP2) {
    // For HTTP/2, we try to read SETTINGS ACK from server to make
    // sure it is really alive, and serving HTTP/2.
    read_ = &LiveCheck::read_clear;
    write_ = &LiveCheck::write_clear;

    if (connection_made() != 0) {
      return -1;
    }

    return 0;
  }

  on_success();

  return 0;
}

int LiveCheck::tls_handshake() {
  conn_.last_read = std::chrono::steady_clock::now();

  ERR_clear_error();

  auto rv = conn_.tls_handshake();

  if (rv == SHRPX_ERR_INPROGRESS) {
    return 0;
  }

  if (rv < 0) {
    return rv;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "SSL/TLS handshake completed";
  }

  if (!get_config()->tls.insecure &&
      tls::check_cert(conn_.tls.ssl, addr_, raddr_) != 0) {
    return -1;
  }

  // Check negotiated ALPN

  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len = 0;

  SSL_get0_alpn_selected(conn_.tls.ssl, &next_proto, &next_proto_len);

  auto proto = StringRef{next_proto, next_proto_len};

  switch (addr_->proto) {
  case Proto::HTTP1:
    if (proto.empty() || proto == "http/1.1"_sr) {
      break;
    }
    return -1;
  case Proto::HTTP2:
    if (util::check_h2_is_selected(proto)) {
      // For HTTP/2, we try to read SETTINGS ACK from server to make
      // sure it is really alive, and serving HTTP/2.
      read_ = &LiveCheck::read_tls;
      write_ = &LiveCheck::write_tls;

      if (connection_made() != 0) {
        return -1;
      }

      return 0;
    }
    return -1;
  default:
    break;
  }

  on_success();

  return 0;
}

int LiveCheck::read_tls() {
  conn_.last_read = std::chrono::steady_clock::now();

  std::array<uint8_t, 4_k> buf;

  ERR_clear_error();

  for (;;) {
    auto nread = conn_.read_tls(buf.data(), buf.size());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return nread;
    }

    if (on_read(buf.data(), nread) != 0) {
      return -1;
    }
  }
}

int LiveCheck::write_tls() {
  conn_.last_read = std::chrono::steady_clock::now();

  ERR_clear_error();

  struct iovec iov;

  for (;;) {
    if (wb_.rleft() > 0) {
      auto iovcnt = wb_.riovec(&iov, 1);
      if (iovcnt != 1) {
        assert(0);
        return -1;
      }
      auto nwrite = conn_.write_tls(iov.iov_base, iov.iov_len);

      if (nwrite == 0) {
        return 0;
      }

      if (nwrite < 0) {
        return nwrite;
      }

      wb_.drain(nwrite);

      continue;
    }

    if (on_write() != 0) {
      return -1;
    }

    if (wb_.rleft() == 0) {
      conn_.start_tls_write_idle();
      break;
    }
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  if (settings_ack_received_) {
    on_success();
  }

  return 0;
}

int LiveCheck::read_clear() {
  conn_.last_read = std::chrono::steady_clock::now();

  std::array<uint8_t, 4_k> buf;

  for (;;) {
    auto nread = conn_.read_clear(buf.data(), buf.size());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return nread;
    }

    if (on_read(buf.data(), nread) != 0) {
      return -1;
    }
  }
}

int LiveCheck::write_clear() {
  conn_.last_read = std::chrono::steady_clock::now();

  struct iovec iov;

  for (;;) {
    if (wb_.rleft() > 0) {
      auto iovcnt = wb_.riovec(&iov, 1);
      if (iovcnt != 1) {
        assert(0);
        return -1;
      }
      auto nwrite = conn_.write_clear(iov.iov_base, iov.iov_len);

      if (nwrite == 0) {
        return 0;
      }

      if (nwrite < 0) {
        return nwrite;
      }

      wb_.drain(nwrite);

      continue;
    }

    if (on_write() != 0) {
      return -1;
    }

    if (wb_.rleft() == 0) {
      break;
    }
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  if (settings_ack_received_) {
    on_success();
  }

  return 0;
}

int LiveCheck::on_read(const uint8_t *data, size_t len) {
  auto rv = nghttp2_session_mem_recv2(session_, data, len);
  if (rv < 0) {
    LOG(ERROR) << "nghttp2_session_mem_recv2() returned error: "
               << nghttp2_strerror(rv);
    return -1;
  }

  if (settings_ack_received_ && !session_closing_) {
    session_closing_ = true;
    auto rv = nghttp2_session_terminate_session(session_, NGHTTP2_NO_ERROR);
    if (rv != 0) {
      return -1;
    }
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wb_.rleft() == 0) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "No more read/write for this session";
    }

    // If we have SETTINGS ACK already, we treat this success.
    if (settings_ack_received_) {
      return 0;
    }

    return -1;
  }

  signal_write();

  return 0;
}

int LiveCheck::on_write() {
  for (;;) {
    const uint8_t *data;
    auto datalen = nghttp2_session_mem_send2(session_, &data);

    if (datalen < 0) {
      LOG(ERROR) << "nghttp2_session_mem_send2() returned error: "
                 << nghttp2_strerror(datalen);
      return -1;
    }
    if (datalen == 0) {
      break;
    }
    wb_.append(data, datalen);

    if (wb_.rleft() >= MAX_BUFFER_SIZE) {
      break;
    }
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wb_.rleft() == 0) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "No more read/write for this session";
    }

    if (settings_ack_received_) {
      return 0;
    }

    return -1;
  }

  return 0;
}

void LiveCheck::on_failure() {
  ++fail_count_;

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Liveness check for " << addr_->host << ":" << addr_->port
              << " failed " << fail_count_ << " time(s) in a row";
  }

  disconnect();

  schedule();
}

void LiveCheck::on_success() {
  ++success_count_;
  fail_count_ = 0;

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Liveness check for " << addr_->host << ":" << addr_->port
              << " succeeded " << success_count_ << " time(s) in a row";
  }

  if (success_count_ < addr_->rise) {
    disconnect();

    schedule();

    return;
  }

  LOG(NOTICE) << util::to_numeric_addr(&addr_->addr) << " is considered online";

  addr_->connect_blocker->online();

  success_count_ = 0;
  fail_count_ = 0;

  disconnect();
}

int LiveCheck::noop() { return 0; }

void LiveCheck::start_settings_timer() {
  auto &downstreamconf = get_config()->http2.downstream;

  ev_timer_set(&settings_timer_, downstreamconf.timeout.settings, 0.);
  ev_timer_start(conn_.loop, &settings_timer_);
}

void LiveCheck::stop_settings_timer() {
  ev_timer_stop(conn_.loop, &settings_timer_);
}

void LiveCheck::settings_ack_received() { settings_ack_received_ = true; }

namespace {
int on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  auto live_check = static_cast<LiveCheck *>(user_data);

  if (frame->hd.type != NGHTTP2_SETTINGS ||
      (frame->hd.flags & NGHTTP2_FLAG_ACK)) {
    return 0;
  }

  live_check->start_settings_timer();

  return 0;
}
} // namespace

namespace {
int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  auto live_check = static_cast<LiveCheck *>(user_data);

  if (frame->hd.type != NGHTTP2_SETTINGS ||
      (frame->hd.flags & NGHTTP2_FLAG_ACK) == 0) {
    return 0;
  }

  live_check->stop_settings_timer();
  live_check->settings_ack_received();

  return 0;
}
} // namespace

int LiveCheck::connection_made() {
  int rv;

  nghttp2_session_callbacks *callbacks;
  rv = nghttp2_session_callbacks_new(&callbacks);
  if (rv != 0) {
    return -1;
  }

  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
                                                       on_frame_send_callback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       on_frame_recv_callback);

  rv = nghttp2_session_client_new(&session_, callbacks, this);

  nghttp2_session_callbacks_del(callbacks);

  if (rv != 0) {
    return -1;
  }

  rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, nullptr, 0);
  if (rv != 0) {
    return -1;
  }

  auto must_terminate =
    addr_->tls && !nghttp2::tls::check_http2_requirement(conn_.tls.ssl);

  if (must_terminate) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "TLSv1.2 was not negotiated. HTTP/2 must not be negotiated.";
    }

    rv =
      nghttp2_session_terminate_session(session_, NGHTTP2_INADEQUATE_SECURITY);
    if (rv != 0) {
      return -1;
    }
  }

  signal_write();

  return 0;
}

void LiveCheck::signal_write() { conn_.wlimit.startw(); }

} // namespace shrpx
