/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#include "shrpx_http2_session.h"

#include <netinet/tcp.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H

#include <vector>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/err.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/err.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include "shrpx_upstream.h"
#include "shrpx_downstream.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_http2_downstream_connection.h"
#include "shrpx_client_handler.h"
#include "shrpx_tls.h"
#include "shrpx_http.h"
#include "shrpx_worker.h"
#include "shrpx_connect_blocker.h"
#include "shrpx_log.h"
#include "http2.h"
#include "util.h"
#include "base64.h"
#include "tls.h"

using namespace nghttp2;

namespace shrpx {

namespace {
constexpr ev_tstamp CONNCHK_TIMEOUT = 5.;
constexpr ev_tstamp CONNCHK_PING_TIMEOUT = 1.;
} // namespace

namespace {
constexpr size_t MAX_BUFFER_SIZE = 32_k;
} // namespace

namespace {
void connchk_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto http2session = static_cast<Http2Session *>(w->data);

  ev_timer_stop(loop, w);

  switch (http2session->get_connection_check_state()) {
  case ConnectionCheck::STARTED:
    // ping timeout; disconnect
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session) << "ping timeout";
    }

    delete http2session;

    return;
  default:
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session) << "connection check required";
    }
    http2session->set_connection_check_state(ConnectionCheck::REQUIRED);
  }
}
} // namespace

namespace {
void settings_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto http2session = static_cast<Http2Session *>(w->data);

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, http2session) << "SETTINGS timeout";
  }

  downstream_failure(http2session->get_addr(), http2session->get_raddr());

  if (http2session->terminate_session(NGHTTP2_SETTINGS_TIMEOUT) != 0) {
    delete http2session;

    return;
  }
  http2session->signal_write();
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto http2session = static_cast<Http2Session *>(conn->data);

  if (w == &conn->rt && !conn->expired_rt()) {
    return;
  }

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, http2session) << "Timeout";
  }

  http2session->on_timeout();

  delete http2session;
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto conn = static_cast<Connection *>(w->data);
  auto http2session = static_cast<Http2Session *>(conn->data);
  rv = http2session->do_read();
  if (rv != 0) {
    delete http2session;

    return;
  }
  http2session->connection_alive();
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  int rv;
  auto conn = static_cast<Connection *>(w->data);
  auto http2session = static_cast<Http2Session *>(conn->data);
  rv = http2session->do_write();
  if (rv != 0) {
    delete http2session;

    return;
  }
  http2session->reset_connection_check_timer_if_not_checking();
}
} // namespace

namespace {
void initiate_connection_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto http2session = static_cast<Http2Session *>(w->data);
  ev_timer_stop(loop, w);
  if (http2session->initiate_connection() != 0) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session) << "Could not initiate backend connection";
    }

    delete http2session;

    return;
  }
}
} // namespace

namespace {
void prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents) {
  auto http2session = static_cast<Http2Session *>(w->data);
  http2session->check_retire();
}
} // namespace

Http2Session::Http2Session(struct ev_loop *loop, SSL_CTX *ssl_ctx,
                           Worker *worker,
                           const std::shared_ptr<DownstreamAddrGroup> &group,
                           DownstreamAddr *addr)
  : dlnext(nullptr),
    dlprev(nullptr),
    conn_(loop, -1, nullptr, worker->get_mcpool(),
          group->shared_addr->timeout.write, group->shared_addr->timeout.read,
          {}, {}, writecb, readcb, timeoutcb, this,
          get_config()->tls.dyn_rec.warmup_threshold,
          get_config()->tls.dyn_rec.idle_timeout, Proto::HTTP2),
    wb_(worker->get_mcpool()),
    worker_(worker),
    ssl_ctx_(ssl_ctx),
    group_(group),
    addr_(addr),
    session_(nullptr),
    raddr_(nullptr),
    state_(Http2SessionState::DISCONNECTED),
    connection_check_state_(ConnectionCheck::NONE),
    freelist_zone_(FreelistZone::NONE),
    settings_recved_(false),
    allow_connect_proto_(false) {
  read_ = write_ = &Http2Session::noop;

  on_read_ = &Http2Session::read_noop;
  on_write_ = &Http2Session::write_noop;

  // We will reuse this many times, so use repeat timeout value.  The
  // timeout value is set later.
  ev_timer_init(&connchk_timer_, connchk_timeout_cb, 0., 0.);

  connchk_timer_.data = this;

  // SETTINGS ACK timeout is 10 seconds for now.  We will reuse this
  // many times, so use repeat timeout value.
  ev_timer_init(&settings_timer_, settings_timeout_cb, 0., 0.);

  settings_timer_.data = this;

  ev_timer_init(&initiate_connection_timer_, initiate_connection_cb, 0., 0.);
  initiate_connection_timer_.data = this;

  ev_prepare_init(&prep_, prepare_cb);
  prep_.data = this;
  ev_prepare_start(loop, &prep_);
}

Http2Session::~Http2Session() {
  exclude_from_scheduling();
  disconnect(should_hard_fail());
}

int Http2Session::disconnect(bool hard) {
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Disconnecting";
  }
  nghttp2_session_del(session_);
  session_ = nullptr;

  wb_.reset();

  if (dns_query_) {
    auto dns_tracker = worker_->get_dns_tracker();
    dns_tracker->cancel(dns_query_.get());
  }

  conn_.rlimit.stopw();
  conn_.wlimit.stopw();

  ev_prepare_stop(conn_.loop, &prep_);

  ev_timer_stop(conn_.loop, &initiate_connection_timer_);
  ev_timer_stop(conn_.loop, &settings_timer_);
  ev_timer_stop(conn_.loop, &connchk_timer_);

  read_ = write_ = &Http2Session::noop;

  on_read_ = &Http2Session::read_noop;
  on_write_ = &Http2Session::write_noop;

  conn_.disconnect();

  if (proxy_htp_) {
    proxy_htp_.reset();
  }

  connection_check_state_ = ConnectionCheck::NONE;
  state_ = Http2SessionState::DISCONNECTED;

  // When deleting Http2DownstreamConnection, it calls this object's
  // remove_downstream_connection().  The multiple
  // Http2DownstreamConnection objects belong to the same
  // ClientHandler object if upstream is h2.  So be careful when you
  // delete ClientHandler here.
  //
  // We allow creating new pending Http2DownstreamConnection with this
  // object.  Upstream::on_downstream_reset() may add
  // Http2DownstreamConnection to another Http2Session.

  for (auto dc = dconns_.head; dc;) {
    auto next = dc->dlnext;
    auto downstream = dc->get_downstream();
    auto upstream = downstream->get_upstream();

    // Failure is allowed only for HTTP/1 upstream where upstream is
    // not shared by multiple Downstreams.
    if (upstream->on_downstream_reset(downstream, hard) != 0) {
      delete upstream->get_client_handler();
    }

    // dc was deleted
    dc = next;
  }

  auto streams = std::move(streams_);
  for (auto s = streams.head; s;) {
    auto next = s->dlnext;
    delete s;
    s = next;
  }

  return 0;
}

int Http2Session::resolve_name() {
  auto dns_query = std::make_unique<DNSQuery>(
    addr_->host, [this](DNSResolverStatus status, const Address *result) {
      int rv;

      if (status == DNSResolverStatus::OK) {
        *resolved_addr_ = *result;
        util::set_port(*this->resolved_addr_, this->addr_->port);
      }

      rv = this->initiate_connection();
      if (rv != 0) {
        delete this;
      }
    });
  resolved_addr_ = std::make_unique<Address>();
  auto dns_tracker = worker_->get_dns_tracker();
  switch (dns_tracker->resolve(resolved_addr_.get(), dns_query.get())) {
  case DNSResolverStatus::ERROR:
    return -1;
  case DNSResolverStatus::RUNNING:
    dns_query_ = std::move(dns_query);
    state_ = Http2SessionState::RESOLVING_NAME;
    return 0;
  case DNSResolverStatus::OK:
    util::set_port(*resolved_addr_, addr_->port);
    return 0;
  default:
    assert(0);
    abort();
  }
}

namespace {
int htp_hdrs_completecb(llhttp_t *htp);
} // namespace

namespace {
constexpr llhttp_settings_t htp_hooks = {
  nullptr,             // llhttp_cb      on_message_begin;
  nullptr,             // llhttp_data_cb on_url;
  nullptr,             // llhttp_data_cb on_status;
  nullptr,             // llhttp_data_cb on_method;
  nullptr,             // llhttp_data_cb on_version;
  nullptr,             // llhttp_data_cb on_header_field;
  nullptr,             // llhttp_data_cb on_header_value;
  nullptr,             // llhttp_data_cb on_chunk_extension_name;
  nullptr,             // llhttp_data_cb on_chunk_extension_value;
  htp_hdrs_completecb, // llhttp_cb      on_headers_complete;
  nullptr,             // llhttp_data_cb on_body;
  nullptr,             // llhttp_cb      on_message_complete;
  nullptr,             // llhttp_cb      on_url_complete;
  nullptr,             // llhttp_cb      on_status_complete;
  nullptr,             // llhttp_cb      on_method_complete;
  nullptr,             // llhttp_cb      on_version_complete;
  nullptr,             // llhttp_cb      on_header_field_complete;
  nullptr,             // llhttp_cb      on_header_value_complete;
  nullptr,             // llhttp_cb      on_chunk_extension_name_complete;
  nullptr,             // llhttp_cb      on_chunk_extension_value_complete;
  nullptr,             // llhttp_cb      on_chunk_header;
  nullptr,             // llhttp_cb      on_chunk_complete;
  nullptr,             // llhttp_cb      on_reset;
};
} // namespace

int Http2Session::initiate_connection() {
  int rv = 0;

  auto worker_blocker = worker_->get_connect_blocker();

  if (state_ == Http2SessionState::DISCONNECTED ||
      state_ == Http2SessionState::RESOLVING_NAME) {
    if (worker_blocker->blocked()) {
      if (LOG_ENABLED(INFO)) {
        SSLOG(INFO, this)
          << "Worker wide backend connection was blocked temporarily";
      }
      return -1;
    }
  }

  auto &downstreamconf = *get_config()->conn.downstream;

  const auto &proxy = get_config()->downstream_http_proxy;
  if (!proxy.host.empty() && state_ == Http2SessionState::DISCONNECTED) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "Connecting to the proxy " << proxy.host << ":"
                        << proxy.port;
    }

    conn_.fd = util::create_nonblock_socket(proxy.addr.su.storage.ss_family);

    if (conn_.fd == -1) {
      auto error = errno;
      SSLOG(WARN, this) << "Backend proxy socket() failed; addr="
                        << util::to_numeric_addr(&proxy.addr)
                        << ", errno=" << error;

      worker_blocker->on_failure();
      return -1;
    }

    rv = connect(conn_.fd, &proxy.addr.su.sa, proxy.addr.len);
    if (rv != 0 && errno != EINPROGRESS) {
      auto error = errno;
      SSLOG(WARN, this) << "Backend proxy connect() failed; addr="
                        << util::to_numeric_addr(&proxy.addr)
                        << ", errno=" << error;

      worker_blocker->on_failure();

      return -1;
    }

    raddr_ = &proxy.addr;

    worker_blocker->on_success();

    ev_io_set(&conn_.rev, conn_.fd, EV_READ);
    ev_io_set(&conn_.wev, conn_.fd, EV_WRITE);

    conn_.wlimit.startw();

    conn_.wt.repeat = downstreamconf.timeout.connect;
    ev_timer_again(conn_.loop, &conn_.wt);

    write_ = &Http2Session::connected;

    on_read_ = &Http2Session::downstream_read_proxy;
    on_write_ = &Http2Session::downstream_connect_proxy;

    proxy_htp_ = std::make_unique<llhttp_t>();
    llhttp_init(proxy_htp_.get(), HTTP_RESPONSE, &htp_hooks);
    proxy_htp_->data = this;

    state_ = Http2SessionState::PROXY_CONNECTING;

    return 0;
  }

  if (state_ == Http2SessionState::DISCONNECTED ||
      state_ == Http2SessionState::PROXY_CONNECTED ||
      state_ == Http2SessionState::RESOLVING_NAME) {
    if (LOG_ENABLED(INFO)) {
      if (state_ != Http2SessionState::RESOLVING_NAME) {
        SSLOG(INFO, this) << "Connecting to downstream server";
      }
    }
    if (addr_->tls) {
      assert(ssl_ctx_);

      if (state_ != Http2SessionState::RESOLVING_NAME) {
        auto ssl = tls::create_ssl(ssl_ctx_);
        if (!ssl) {
          return -1;
        }

        tls::setup_downstream_http2_alpn(ssl);

        conn_.set_ssl(ssl);
        conn_.tls.client_session_cache = &addr_->tls_session_cache;

        auto sni_name =
          addr_->sni.empty() ? StringRef{addr_->host} : StringRef{addr_->sni};

        if (!util::numeric_host(sni_name.data())) {
          // TLS extensions: SNI. There is no documentation about the return
          // code for this function (actually this is macro wrapping SSL_ctrl
          // at the time of this writing).
          SSL_set_tlsext_host_name(conn_.tls.ssl, sni_name.data());
        }

        auto tls_session = tls::reuse_tls_session(addr_->tls_session_cache);
        if (tls_session) {
          SSL_set_session(conn_.tls.ssl, tls_session);
          SSL_SESSION_free(tls_session);
        }
      }

      if (state_ == Http2SessionState::DISCONNECTED) {
        if (addr_->dns) {
          rv = resolve_name();
          if (rv != 0) {
            downstream_failure(addr_, nullptr);
            return -1;
          }
          if (state_ == Http2SessionState::RESOLVING_NAME) {
            return 0;
          }
          raddr_ = resolved_addr_.get();
        } else {
          raddr_ = &addr_->addr;
        }
      }

      if (state_ == Http2SessionState::RESOLVING_NAME) {
        if (dns_query_->status == DNSResolverStatus::ERROR) {
          downstream_failure(addr_, nullptr);
          return -1;
        }
        assert(dns_query_->status == DNSResolverStatus::OK);
        state_ = Http2SessionState::DISCONNECTED;
        dns_query_.reset();
        raddr_ = resolved_addr_.get();
      }

      // If state_ == Http2SessionState::PROXY_CONNECTED, we have
      // connected to the proxy using conn_.fd and tunnel has been
      // established.
      if (state_ == Http2SessionState::DISCONNECTED) {
        assert(conn_.fd == -1);

        conn_.fd = util::create_nonblock_socket(raddr_->su.storage.ss_family);
        if (conn_.fd == -1) {
          auto error = errno;
          SSLOG(WARN, this)
            << "socket() failed; addr=" << util::to_numeric_addr(raddr_)
            << ", errno=" << error;

          worker_blocker->on_failure();
          return -1;
        }

        worker_blocker->on_success();

        rv = connect(conn_.fd,
                     // TODO maybe not thread-safe?
                     const_cast<sockaddr *>(&raddr_->su.sa), raddr_->len);
        if (rv != 0 && errno != EINPROGRESS) {
          auto error = errno;
          SSLOG(WARN, this)
            << "connect() failed; addr=" << util::to_numeric_addr(raddr_)
            << ", errno=" << error;

          downstream_failure(addr_, raddr_);
          return -1;
        }

        ev_io_set(&conn_.rev, conn_.fd, EV_READ);
        ev_io_set(&conn_.wev, conn_.fd, EV_WRITE);
      }

      conn_.prepare_client_handshake();
    } else {
      if (state_ == Http2SessionState::DISCONNECTED) {
        // Without TLS and proxy.
        if (addr_->dns) {
          rv = resolve_name();
          if (rv != 0) {
            downstream_failure(addr_, nullptr);
            return -1;
          }
          if (state_ == Http2SessionState::RESOLVING_NAME) {
            return 0;
          }
          raddr_ = resolved_addr_.get();
        } else {
          raddr_ = &addr_->addr;
        }
      }

      if (state_ == Http2SessionState::RESOLVING_NAME) {
        if (dns_query_->status == DNSResolverStatus::ERROR) {
          downstream_failure(addr_, nullptr);
          return -1;
        }
        assert(dns_query_->status == DNSResolverStatus::OK);
        state_ = Http2SessionState::DISCONNECTED;
        dns_query_.reset();
        raddr_ = resolved_addr_.get();
      }

      if (state_ == Http2SessionState::DISCONNECTED) {
        // Without TLS and proxy.
        assert(conn_.fd == -1);

        conn_.fd = util::create_nonblock_socket(raddr_->su.storage.ss_family);

        if (conn_.fd == -1) {
          auto error = errno;
          SSLOG(WARN, this)
            << "socket() failed; addr=" << util::to_numeric_addr(raddr_)
            << ", errno=" << error;

          worker_blocker->on_failure();
          return -1;
        }

        worker_blocker->on_success();

        rv = connect(conn_.fd, const_cast<sockaddr *>(&raddr_->su.sa),
                     raddr_->len);
        if (rv != 0 && errno != EINPROGRESS) {
          auto error = errno;
          SSLOG(WARN, this)
            << "connect() failed; addr=" << util::to_numeric_addr(raddr_)
            << ", errno=" << error;

          downstream_failure(addr_, raddr_);
          return -1;
        }

        ev_io_set(&conn_.rev, conn_.fd, EV_READ);
        ev_io_set(&conn_.wev, conn_.fd, EV_WRITE);
      }
    }

    // We have been already connected when no TLS and proxy is used.
    if (state_ == Http2SessionState::PROXY_CONNECTED) {
      on_read_ = &Http2Session::read_noop;
      on_write_ = &Http2Session::write_noop;

      return connected();
    }

    write_ = &Http2Session::connected;

    state_ = Http2SessionState::CONNECTING;
    conn_.wlimit.startw();

    conn_.wt.repeat = downstreamconf.timeout.connect;
    ev_timer_again(conn_.loop, &conn_.wt);

    return 0;
  }

  // Unreachable
  assert(0);

  return 0;
}

namespace {
int htp_hdrs_completecb(llhttp_t *htp) {
  auto http2session = static_cast<Http2Session *>(htp->data);

  // We only read HTTP header part.  If tunneling succeeds, response
  // body is a different protocol (HTTP/2 in this case), we don't read
  // them here.

  // We just check status code here
  if (htp->status_code / 100 == 2) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session) << "Tunneling success";
    }
    http2session->set_state(Http2SessionState::PROXY_CONNECTED);

    return HPE_PAUSED;
  }

  SSLOG(WARN, http2session) << "Tunneling failed: " << htp->status_code;
  http2session->set_state(Http2SessionState::PROXY_FAILED);

  return HPE_PAUSED;
}
} // namespace

int Http2Session::downstream_read_proxy(const uint8_t *data, size_t datalen) {
  auto htperr = llhttp_execute(proxy_htp_.get(),
                               reinterpret_cast<const char *>(data), datalen);
  if (htperr == HPE_PAUSED) {
    switch (state_) {
    case Http2SessionState::PROXY_CONNECTED:
      // Initiate SSL/TLS handshake through established tunnel.
      if (initiate_connection() != 0) {
        return -1;
      }
      return 0;
    case Http2SessionState::PROXY_FAILED:
      return -1;
    default:
      break;
    }
    // should not be here
    assert(0);
  }

  if (htperr != HPE_OK) {
    return -1;
  }

  return 0;
}

int Http2Session::downstream_connect_proxy() {
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Connected to the proxy";
  }

  std::string req = "CONNECT ";
  req.append(addr_->hostport.data(), addr_->hostport.size());
  if (addr_->port == 80 || addr_->port == 443) {
    req += ':';
    req += util::utos(addr_->port);
  }
  req += " HTTP/1.1\r\nHost: ";
  req += addr_->host;
  req += "\r\n";
  const auto &proxy = get_config()->downstream_http_proxy;
  if (!proxy.userinfo.empty()) {
    req += "Proxy-Authorization: Basic ";
    req += base64::encode(std::begin(proxy.userinfo), std::end(proxy.userinfo));
    req += "\r\n";
  }
  req += "\r\n";
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "HTTP proxy request headers\n" << req;
  }
  wb_.append(req);

  on_write_ = &Http2Session::write_noop;

  signal_write();
  return 0;
}

void Http2Session::add_downstream_connection(Http2DownstreamConnection *dconn) {
  dconns_.append(dconn);
  ++addr_->num_dconn;
}

void Http2Session::remove_downstream_connection(
  Http2DownstreamConnection *dconn) {
  --addr_->num_dconn;
  dconns_.remove(dconn);
  dconn->detach_stream_data();

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Remove downstream";
  }

  if (freelist_zone_ == FreelistZone::NONE && !max_concurrency_reached()) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "Append to http2_extra_freelist, addr=" << addr_
                        << ", freelist.size="
                        << addr_->http2_extra_freelist.size();
    }

    add_to_extra_freelist();
  }
}

void Http2Session::remove_stream_data(StreamData *sd) {
  streams_.remove(sd);
  if (sd->dconn) {
    sd->dconn->detach_stream_data();
  }
  delete sd;
}

int Http2Session::submit_request(Http2DownstreamConnection *dconn,
                                 const nghttp2_nv *nva, size_t nvlen,
                                 const nghttp2_data_provider2 *data_prd) {
  assert(state_ == Http2SessionState::CONNECTED);
  auto sd = std::make_unique<StreamData>();
  sd->dlnext = sd->dlprev = nullptr;
  // TODO Specify nullptr to pri_spec for now
  auto stream_id =
    nghttp2_submit_request2(session_, nullptr, nva, nvlen, data_prd, sd.get());
  if (stream_id < 0) {
    SSLOG(FATAL, this) << "nghttp2_submit_request2() failed: "
                       << nghttp2_strerror(stream_id);
    return -1;
  }

  dconn->attach_stream_data(sd.get());
  dconn->get_downstream()->set_downstream_stream_id(stream_id);
  streams_.append(sd.release());

  return 0;
}

int Http2Session::submit_rst_stream(int32_t stream_id, uint32_t error_code) {
  assert(state_ == Http2SessionState::CONNECTED);
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "RST_STREAM stream_id=" << stream_id
                      << " with error_code=" << error_code;
  }
  int rv = nghttp2_submit_rst_stream(session_, NGHTTP2_FLAG_NONE, stream_id,
                                     error_code);
  if (rv != 0) {
    SSLOG(FATAL, this) << "nghttp2_submit_rst_stream() failed: "
                       << nghttp2_strerror(rv);
    return -1;
  }
  return 0;
}

nghttp2_session *Http2Session::get_session() const { return session_; }

int Http2Session::resume_data(Http2DownstreamConnection *dconn) {
  assert(state_ == Http2SessionState::CONNECTED);
  auto downstream = dconn->get_downstream();
  int rv = nghttp2_session_resume_data(session_,
                                       downstream->get_downstream_stream_id());
  switch (rv) {
  case 0:
  case NGHTTP2_ERR_INVALID_ARGUMENT:
    return 0;
  default:
    SSLOG(FATAL, this) << "nghttp2_resume_session() failed: "
                       << nghttp2_strerror(rv);
    return -1;
  }
}

namespace {
void call_downstream_readcb(Http2Session *http2session,
                            Downstream *downstream) {
  auto upstream = downstream->get_upstream();
  if (!upstream) {
    return;
  }
  if (upstream->downstream_read(downstream->get_downstream_connection()) != 0) {
    delete upstream->get_client_handler();
  }
}
} // namespace

namespace {
int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, http2session)
      << "Stream stream_id=" << stream_id << " is being closed with error code "
      << error_code;
  }
  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, stream_id));
  if (sd == 0) {
    // We might get this close callback when pushed streams are
    // closed.
    return 0;
  }
  auto dconn = sd->dconn;
  if (dconn) {
    auto downstream = dconn->get_downstream();
    auto upstream = downstream->get_upstream();

    if (downstream->get_downstream_stream_id() % 2 == 0 &&
        downstream->get_request_state() == DownstreamState::INITIAL) {
      // Downstream is canceled in backend before it is submitted in
      // frontend session.

      // This will avoid to send RST_STREAM to backend
      downstream->set_response_state(DownstreamState::MSG_RESET);
      upstream->cancel_premature_downstream(downstream);
    } else {
      if (downstream->get_upgraded() && downstream->get_response_state() ==
                                          DownstreamState::HEADER_COMPLETE) {
        // For tunneled connection, we have to submit RST_STREAM to
        // upstream *after* whole response body is sent. We just set
        // MSG_COMPLETE here. Upstream will take care of that.
        downstream->get_upstream()->on_downstream_body_complete(downstream);
        downstream->set_response_state(DownstreamState::MSG_COMPLETE);
      } else if (error_code == NGHTTP2_NO_ERROR) {
        switch (downstream->get_response_state()) {
        case DownstreamState::MSG_COMPLETE:
        case DownstreamState::MSG_BAD_HEADER:
          break;
        default:
          downstream->set_response_state(DownstreamState::MSG_RESET);
        }
      } else if (downstream->get_response_state() !=
                 DownstreamState::MSG_BAD_HEADER) {
        downstream->set_response_state(DownstreamState::MSG_RESET);
      }
      if (downstream->get_response_state() == DownstreamState::MSG_RESET &&
          downstream->get_response_rst_stream_error_code() ==
            NGHTTP2_NO_ERROR) {
        downstream->set_response_rst_stream_error_code(error_code);
      }
      call_downstream_readcb(http2session, downstream);
    }
    // dconn may be deleted
  }
  // The life time of StreamData ends here
  http2session->remove_stream_data(sd);
  return 0;
}
} // namespace

void Http2Session::start_settings_timer() {
  auto &downstreamconf = get_config()->http2.downstream;

  ev_timer_set(&settings_timer_, downstreamconf.timeout.settings, 0.);
  ev_timer_start(conn_.loop, &settings_timer_);
}

void Http2Session::stop_settings_timer() {
  ev_timer_stop(conn_.loop, &settings_timer_);
}

namespace {
int on_header_callback2(nghttp2_session *session, const nghttp2_frame *frame,
                        nghttp2_rcbuf *name, nghttp2_rcbuf *value,
                        uint8_t flags, void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);
  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  if (!sd || !sd->dconn) {
    return 0;
  }
  auto downstream = sd->dconn->get_downstream();

  auto namebuf = nghttp2_rcbuf_get_buf(name);
  auto valuebuf = nghttp2_rcbuf_get_buf(value);

  auto &resp = downstream->response();
  auto &httpconf = get_config()->http;

  switch (frame->hd.type) {
  case NGHTTP2_HEADERS: {
    auto trailer = frame->headers.cat == NGHTTP2_HCAT_HEADERS &&
                   !downstream->get_expect_final_response();

    if (resp.fs.buffer_size() + namebuf.len + valuebuf.len >
          httpconf.response_header_field_buffer ||
        resp.fs.num_fields() >= httpconf.max_response_header_fields) {
      if (LOG_ENABLED(INFO)) {
        DLOG(INFO, downstream)
          << "Too large or many header field size="
          << resp.fs.buffer_size() + namebuf.len + valuebuf.len
          << ", num=" << resp.fs.num_fields() + 1;
      }

      if (trailer) {
        // We don't care trailer part exceeds header size limit; just
        // discard it.
        return 0;
      }

      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    auto nameref = StringRef{namebuf.base, namebuf.len};
    auto valueref = StringRef{valuebuf.base, valuebuf.len};
    auto token = http2::lookup_token(nameref);
    auto no_index = flags & NGHTTP2_NV_FLAG_NO_INDEX;

    downstream->add_rcbuf(name);
    downstream->add_rcbuf(value);

    if (trailer) {
      // just store header fields for trailer part
      resp.fs.add_trailer_token(nameref, valueref, no_index, token);
      return 0;
    }

    resp.fs.add_header_token(nameref, valueref, no_index, token);
    return 0;
  }
  case NGHTTP2_PUSH_PROMISE: {
    auto promised_stream_id = frame->push_promise.promised_stream_id;
    auto promised_sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, promised_stream_id));
    if (!promised_sd || !promised_sd->dconn) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    auto promised_downstream = promised_sd->dconn->get_downstream();

    auto namebuf = nghttp2_rcbuf_get_buf(name);
    auto valuebuf = nghttp2_rcbuf_get_buf(value);

    assert(promised_downstream);

    auto &promised_req = promised_downstream->request();

    // We use request header limit for PUSH_PROMISE
    if (promised_req.fs.buffer_size() + namebuf.len + valuebuf.len >
          httpconf.request_header_field_buffer ||
        promised_req.fs.num_fields() >= httpconf.max_request_header_fields) {
      if (LOG_ENABLED(INFO)) {
        DLOG(INFO, downstream)
          << "Too large or many header field size="
          << promised_req.fs.buffer_size() + namebuf.len + valuebuf.len
          << ", num=" << promised_req.fs.num_fields() + 1;
      }

      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    promised_downstream->add_rcbuf(name);
    promised_downstream->add_rcbuf(value);

    auto nameref = StringRef{namebuf.base, namebuf.len};
    auto valueref = StringRef{valuebuf.base, valuebuf.len};
    auto token = http2::lookup_token(nameref);
    promised_req.fs.add_header_token(nameref, valueref,
                                     flags & NGHTTP2_NV_FLAG_NO_INDEX, token);

    return 0;
  }
  }

  return 0;
}
} // namespace

namespace {
int on_invalid_header_callback2(nghttp2_session *session,
                                const nghttp2_frame *frame, nghttp2_rcbuf *name,
                                nghttp2_rcbuf *value, uint8_t flags,
                                void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);
  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  if (!sd || !sd->dconn) {
    return 0;
  }

  int32_t stream_id;

  if (frame->hd.type == NGHTTP2_PUSH_PROMISE) {
    stream_id = frame->push_promise.promised_stream_id;
  } else {
    stream_id = frame->hd.stream_id;
  }

  if (LOG_ENABLED(INFO)) {
    auto namebuf = nghttp2_rcbuf_get_buf(name);
    auto valuebuf = nghttp2_rcbuf_get_buf(value);

    SSLOG(INFO, http2session)
      << "Invalid header field for stream_id=" << stream_id
      << " in frame type=" << static_cast<uint32_t>(frame->hd.type)
      << ": name=[" << StringRef{namebuf.base, namebuf.len} << "], value=["
      << StringRef{valuebuf.base, valuebuf.len} << "]";
  }

  http2session->submit_rst_stream(stream_id, NGHTTP2_PROTOCOL_ERROR);

  return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
}
} // namespace

namespace {
int on_begin_headers_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);

  switch (frame->hd.type) {
  case NGHTTP2_HEADERS: {
    if (frame->headers.cat != NGHTTP2_HCAT_RESPONSE &&
        frame->headers.cat != NGHTTP2_HCAT_PUSH_RESPONSE) {
      return 0;
    }
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!sd || !sd->dconn) {
      http2session->submit_rst_stream(frame->hd.stream_id,
                                      NGHTTP2_INTERNAL_ERROR);
      return 0;
    }
    return 0;
  }
  case NGHTTP2_PUSH_PROMISE: {
    auto promised_stream_id = frame->push_promise.promised_stream_id;
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!sd || !sd->dconn) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    auto downstream = sd->dconn->get_downstream();

    assert(downstream);
    assert(downstream->get_downstream_stream_id() == frame->hd.stream_id);

    if (http2session->handle_downstream_push_promise(downstream,
                                                     promised_stream_id) != 0) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    return 0;
  }
  }

  return 0;
}
} // namespace

namespace {
int on_response_headers(Http2Session *http2session, Downstream *downstream,
                        nghttp2_session *session, const nghttp2_frame *frame) {
  int rv;

  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  const auto &req = downstream->request();
  auto &resp = downstream->response();

  auto &nva = resp.fs.headers();

  auto config = get_config();
  auto &loggingconf = config->logging;

  downstream->set_expect_final_response(false);

  auto status = resp.fs.header(http2::HD__STATUS);
  // libnghttp2 guarantees this exists and can be parsed
  assert(status);
  auto status_code = http2::parse_http_status_code(status->value);

  resp.http_status = status_code;
  resp.http_major = 2;
  resp.http_minor = 0;

  downstream->set_downstream_addr_group(
    http2session->get_downstream_addr_group());
  downstream->set_addr(http2session->get_addr());

  if (LOG_ENABLED(INFO)) {
    std::stringstream ss;
    for (auto &nv : nva) {
      ss << TTY_HTTP_HD << nv.name << TTY_RST << ": " << nv.value << "\n";
    }
    SSLOG(INFO, http2session)
      << "HTTP response headers. stream_id=" << frame->hd.stream_id << "\n"
      << ss.str();
  }

  if (downstream->get_non_final_response()) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session) << "This is non-final response.";
    }

    downstream->set_expect_final_response(true);
    rv = upstream->on_downstream_header_complete(downstream);

    // Now Dowstream's response headers are erased.

    if (rv != 0) {
      http2session->submit_rst_stream(frame->hd.stream_id,
                                      NGHTTP2_PROTOCOL_ERROR);
      downstream->set_response_state(DownstreamState::MSG_RESET);
    }

    return 0;
  }

  downstream->set_response_state(DownstreamState::HEADER_COMPLETE);
  downstream->check_upgrade_fulfilled_http2();

  if (downstream->get_upgraded()) {
    resp.connection_close = true;
    // On upgrade success, both ends can send data
    if (upstream->resume_read(SHRPX_NO_BUFFER, downstream, 0) != 0) {
      // If resume_read fails, just drop connection. Not ideal.
      delete handler;
      return -1;
    }
    downstream->set_request_state(DownstreamState::HEADER_COMPLETE);
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session)
        << "HTTP upgrade success. stream_id=" << frame->hd.stream_id;
    }
  } else {
    auto content_length = resp.fs.header(http2::HD_CONTENT_LENGTH);
    if (content_length) {
      // libnghttp2 guarantees this can be parsed
      resp.fs.content_length =
        util::parse_uint(content_length->value).value_or(-1);
    }

    if (resp.fs.content_length == -1 && downstream->expect_response_body()) {
      // Here we have response body but Content-Length is not known in
      // advance.
      if (req.http_major <= 0 || (req.http_major == 1 && req.http_minor == 0)) {
        // We simply close connection for pre-HTTP/1.1 in this case.
        resp.connection_close = true;
      } else {
        // Otherwise, use chunked encoding to keep upstream connection
        // open.  In HTTP2, we are supposed not to receive
        // transfer-encoding.
        resp.fs.add_header_token("transfer-encoding"_sr, "chunked"_sr, false,
                                 http2::HD_TRANSFER_ENCODING);
        downstream->set_chunked_response(true);
      }
    }
  }

  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
    resp.headers_only = true;
  }

  if (loggingconf.access.write_early && downstream->accesslog_ready()) {
    handler->write_accesslog(downstream);
    downstream->set_accesslog_written(true);
  }

  rv = upstream->on_downstream_header_complete(downstream);
  if (rv != 0) {
    // Handling early return (in other words, response was hijacked by
    // mruby scripting).
    if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
      http2session->submit_rst_stream(frame->hd.stream_id, NGHTTP2_CANCEL);
    } else {
      http2session->submit_rst_stream(frame->hd.stream_id,
                                      NGHTTP2_INTERNAL_ERROR);
      downstream->set_response_state(DownstreamState::MSG_RESET);
    }
  }

  return 0;
}
} // namespace

namespace {
int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  int rv;
  auto http2session = static_cast<Http2Session *>(user_data);

  switch (frame->hd.type) {
  case NGHTTP2_DATA: {
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!sd || !sd->dconn) {
      return 0;
    }
    auto downstream = sd->dconn->get_downstream();
    auto upstream = downstream->get_upstream();
    rv = upstream->on_downstream_body(downstream, nullptr, 0, true);
    if (rv != 0) {
      http2session->submit_rst_stream(frame->hd.stream_id,
                                      NGHTTP2_INTERNAL_ERROR);
      downstream->set_response_state(DownstreamState::MSG_RESET);

    } else if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      downstream->disable_downstream_rtimer();

      if (downstream->get_response_state() ==
          DownstreamState::HEADER_COMPLETE) {
        downstream->set_response_state(DownstreamState::MSG_COMPLETE);

        rv = upstream->on_downstream_body_complete(downstream);

        if (rv != 0) {
          downstream->set_response_state(DownstreamState::MSG_RESET);
        }
      }
    }

    call_downstream_readcb(http2session, downstream);
    return 0;
  }
  case NGHTTP2_HEADERS: {
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!sd || !sd->dconn) {
      return 0;
    }
    auto downstream = sd->dconn->get_downstream();

    if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE ||
        frame->headers.cat == NGHTTP2_HCAT_PUSH_RESPONSE) {
      rv = on_response_headers(http2session, downstream, session, frame);

      if (rv != 0) {
        return 0;
      }
    } else if (frame->headers.cat == NGHTTP2_HCAT_HEADERS) {
      if (downstream->get_expect_final_response()) {
        rv = on_response_headers(http2session, downstream, session, frame);

        if (rv != 0) {
          return 0;
        }
      }
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      downstream->disable_downstream_rtimer();

      if (downstream->get_response_state() ==
          DownstreamState::HEADER_COMPLETE) {
        downstream->set_response_state(DownstreamState::MSG_COMPLETE);

        auto upstream = downstream->get_upstream();

        rv = upstream->on_downstream_body_complete(downstream);

        if (rv != 0) {
          downstream->set_response_state(DownstreamState::MSG_RESET);
        }
      }
    } else {
      downstream->reset_downstream_rtimer();
    }

    // This may delete downstream
    call_downstream_readcb(http2session, downstream);

    return 0;
  }
  case NGHTTP2_RST_STREAM: {
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (sd && sd->dconn) {
      auto downstream = sd->dconn->get_downstream();
      downstream->set_response_rst_stream_error_code(
        frame->rst_stream.error_code);
      call_downstream_readcb(http2session, downstream);
    }
    return 0;
  }
  case NGHTTP2_SETTINGS: {
    if ((frame->hd.flags & NGHTTP2_FLAG_ACK) == 0) {
      http2session->on_settings_received(frame);
      return 0;
    }

    http2session->stop_settings_timer();

    auto addr = http2session->get_addr();
    auto &connect_blocker = addr->connect_blocker;

    connect_blocker->on_success();

    return 0;
  }
  case NGHTTP2_PING:
    if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "PING ACK received";
      }
      http2session->connection_alive();
    }
    return 0;
  case NGHTTP2_PUSH_PROMISE: {
    auto promised_stream_id = frame->push_promise.promised_stream_id;

    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, http2session)
        << "Received downstream PUSH_PROMISE stream_id=" << frame->hd.stream_id
        << ", promised_stream_id=" << promised_stream_id;
    }

    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
    if (!sd || !sd->dconn) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return 0;
    }

    auto downstream = sd->dconn->get_downstream();

    assert(downstream);
    assert(downstream->get_downstream_stream_id() == frame->hd.stream_id);

    auto promised_sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, promised_stream_id));
    if (!promised_sd || !promised_sd->dconn) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return 0;
    }

    auto promised_downstream = promised_sd->dconn->get_downstream();

    assert(promised_downstream);

    if (http2session->handle_downstream_push_promise_complete(
          downstream, promised_downstream) != 0) {
      http2session->submit_rst_stream(promised_stream_id, NGHTTP2_CANCEL);
      return 0;
    }

    return 0;
  }
  case NGHTTP2_GOAWAY:
    if (LOG_ENABLED(INFO)) {
      auto debug_data = util::ascii_dump(frame->goaway.opaque_data,
                                         frame->goaway.opaque_data_len);

      SSLOG(INFO, http2session)
        << "GOAWAY received: last-stream-id=" << frame->goaway.last_stream_id
        << ", error_code=" << frame->goaway.error_code
        << ", debug_data=" << debug_data;
    }
    return 0;
  default:
    return 0;
  }
}
} // namespace

namespace {
int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags,
                                int32_t stream_id, const uint8_t *data,
                                size_t len, void *user_data) {
  int rv;
  auto http2session = static_cast<Http2Session *>(user_data);
  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, stream_id));
  if (!sd || !sd->dconn) {
    http2session->submit_rst_stream(stream_id, NGHTTP2_INTERNAL_ERROR);

    if (http2session->consume(stream_id, len) != 0) {
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    return 0;
  }
  auto downstream = sd->dconn->get_downstream();
  if (!downstream->expect_response_body()) {
    http2session->submit_rst_stream(stream_id, NGHTTP2_INTERNAL_ERROR);

    if (http2session->consume(stream_id, len) != 0) {
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    return 0;
  }

  // We don't want DATA after non-final response, which is illegal in
  // HTTP.
  if (downstream->get_non_final_response()) {
    http2session->submit_rst_stream(stream_id, NGHTTP2_PROTOCOL_ERROR);

    if (http2session->consume(stream_id, len) != 0) {
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    return 0;
  }

  downstream->reset_downstream_rtimer();

  auto &resp = downstream->response();

  resp.recv_body_length += len;
  resp.unconsumed_body_length += len;

  auto upstream = downstream->get_upstream();
  rv = upstream->on_downstream_body(downstream, data, len, false);
  if (rv != 0) {
    http2session->submit_rst_stream(stream_id, NGHTTP2_INTERNAL_ERROR);

    if (http2session->consume(stream_id, len) != 0) {
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    downstream->set_response_state(DownstreamState::MSG_RESET);
  }

  call_downstream_readcb(http2session, downstream);
  return 0;
}
} // namespace

namespace {
int on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);

  if (frame->hd.type == NGHTTP2_DATA || frame->hd.type == NGHTTP2_HEADERS) {
    auto sd = static_cast<StreamData *>(
      nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));

    if (!sd || !sd->dconn) {
      return 0;
    }

    auto downstream = sd->dconn->get_downstream();

    if (frame->hd.type == NGHTTP2_HEADERS &&
        frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
      downstream->set_request_header_sent(true);
      auto src = downstream->get_blocked_request_buf();
      if (src->rleft()) {
        auto dest = downstream->get_request_buf();
        src->remove(*dest);
        if (http2session->resume_data(sd->dconn) != 0) {
          return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
        downstream->ensure_downstream_wtimer();
      }
    }

    if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == 0) {
      return 0;
    }

    downstream->reset_downstream_rtimer();

    return 0;
  }

  if (frame->hd.type == NGHTTP2_SETTINGS &&
      (frame->hd.flags & NGHTTP2_FLAG_ACK) == 0) {
    http2session->start_settings_timer();
  }
  return 0;
}
} // namespace

namespace {
int on_frame_not_send_callback(nghttp2_session *session,
                               const nghttp2_frame *frame, int lib_error_code,
                               void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);
  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, http2session) << "Failed to send control frame type="
                              << static_cast<uint32_t>(frame->hd.type)
                              << ", lib_error_code=" << lib_error_code << ": "
                              << nghttp2_strerror(lib_error_code);
  }
  if (frame->hd.type != NGHTTP2_HEADERS ||
      lib_error_code == NGHTTP2_ERR_STREAM_CLOSED ||
      lib_error_code == NGHTTP2_ERR_STREAM_CLOSING) {
    return 0;
  }

  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));
  if (!sd) {
    return 0;
  }
  if (!sd->dconn) {
    return 0;
  }
  auto downstream = sd->dconn->get_downstream();

  if (lib_error_code == NGHTTP2_ERR_START_STREAM_NOT_ALLOWED) {
    // Migrate to another downstream connection.
    auto upstream = downstream->get_upstream();

    if (upstream->on_downstream_reset(downstream, false)) {
      // This should be done for h1 upstream only.  Deleting
      // ClientHandler for h2 upstream may lead to crash.
      delete upstream->get_client_handler();
    }

    return 0;
  }

  // To avoid stream hanging around, flag DownstreamState::MSG_RESET.
  downstream->set_response_state(DownstreamState::MSG_RESET);
  call_downstream_readcb(http2session, downstream);

  return 0;
}
} // namespace

namespace {
constexpr auto PADDING = std::array<uint8_t, 256>{};
} // namespace

namespace {
int send_data_callback(nghttp2_session *session, nghttp2_frame *frame,
                       const uint8_t *framehd, size_t length,
                       nghttp2_data_source *source, void *user_data) {
  auto http2session = static_cast<Http2Session *>(user_data);
  auto sd = static_cast<StreamData *>(
    nghttp2_session_get_stream_user_data(session, frame->hd.stream_id));

  if (sd == nullptr) {
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
  }

  auto dconn = sd->dconn;
  auto downstream = dconn->get_downstream();
  auto input = downstream->get_request_buf();
  auto wb = http2session->get_request_buf();

  size_t padlen = 0;

  wb->append(framehd, 9);
  if (frame->data.padlen > 0) {
    padlen = frame->data.padlen - 1;
    wb->append(static_cast<uint8_t>(padlen));
  }

  input->remove(*wb, length);

  wb->append(PADDING.data(), padlen);

  if (input->rleft() == 0) {
    downstream->disable_downstream_wtimer();
  } else {
    downstream->reset_downstream_wtimer();
  }

  if (length > 0) {
    // This is important because it will handle flow control
    // stuff.
    if (downstream->get_upstream()->resume_read(SHRPX_NO_BUFFER, downstream,
                                                length) != 0) {
      // In this case, downstream may be deleted.
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    // Here sd->dconn could be nullptr, because
    // Upstream::resume_read() may delete downstream which will delete
    // dconn.  Is this still really true?
  }

  return 0;
}
} // namespace

nghttp2_session_callbacks *create_http2_downstream_callbacks() {
  int rv;
  nghttp2_session_callbacks *callbacks;

  rv = nghttp2_session_callbacks_new(&callbacks);

  if (rv != 0) {
    return nullptr;
  }

  nghttp2_session_callbacks_set_on_stream_close_callback(
    callbacks, on_stream_close_callback);

  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       on_frame_recv_callback);

  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
    callbacks, on_data_chunk_recv_callback);

  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
                                                       on_frame_send_callback);

  nghttp2_session_callbacks_set_on_frame_not_send_callback(
    callbacks, on_frame_not_send_callback);

  nghttp2_session_callbacks_set_on_header_callback2(callbacks,
                                                    on_header_callback2);

  nghttp2_session_callbacks_set_on_invalid_header_callback2(
    callbacks, on_invalid_header_callback2);

  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, on_begin_headers_callback);

  nghttp2_session_callbacks_set_send_data_callback(callbacks,
                                                   send_data_callback);

  if (get_config()->padding) {
    nghttp2_session_callbacks_set_select_padding_callback2(
      callbacks, http::select_padding_callback);
  }

  return callbacks;
}

int Http2Session::connection_made() {
  int rv;

  state_ = Http2SessionState::CONNECTED;

  on_write_ = &Http2Session::downstream_write;
  on_read_ = &Http2Session::downstream_read;

  if (addr_->tls) {
    const unsigned char *next_proto = nullptr;
    unsigned int next_proto_len = 0;

    SSL_get0_alpn_selected(conn_.tls.ssl, &next_proto, &next_proto_len);

    if (!next_proto) {
      downstream_failure(addr_, raddr_);
      return -1;
    }

    auto proto = StringRef{next_proto, next_proto_len};
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "Negotiated next protocol: " << proto;
    }
    if (!util::check_h2_is_selected(proto)) {
      downstream_failure(addr_, raddr_);
      return -1;
    }
  }

  auto config = get_config();
  auto &http2conf = config->http2;

  rv = nghttp2_session_client_new2(&session_, http2conf.downstream.callbacks,
                                   this, http2conf.downstream.option);

  if (rv != 0) {
    return -1;
  }

  std::array<nghttp2_settings_entry, 5> entry;
  size_t nentry = 3;
  entry[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  entry[0].value = http2conf.downstream.max_concurrent_streams;

  entry[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  entry[1].value = http2conf.downstream.window_size;

  entry[2].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  entry[2].value = 1;

  if (http2conf.no_server_push || config->http2_proxy) {
    entry[nentry].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
    entry[nentry].value = 0;
    ++nentry;
  }

  if (http2conf.downstream.decoder_dynamic_table_size !=
      NGHTTP2_DEFAULT_HEADER_TABLE_SIZE) {
    entry[nentry].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
    entry[nentry].value = http2conf.downstream.decoder_dynamic_table_size;
    ++nentry;
  }

  rv =
    nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, entry.data(), nentry);
  if (rv != 0) {
    return -1;
  }

  rv = nghttp2_session_set_local_window_size(
    session_, NGHTTP2_FLAG_NONE, 0,
    http2conf.downstream.connection_window_size);
  if (rv != 0) {
    return -1;
  }

  reset_connection_check_timer(CONNCHK_TIMEOUT);

  submit_pending_requests();

  signal_write();
  return 0;
}

int Http2Session::do_read() { return read_(*this); }
int Http2Session::do_write() { return write_(*this); }

int Http2Session::on_read(const uint8_t *data, size_t datalen) {
  return on_read_(*this, data, datalen);
}

int Http2Session::on_write() { return on_write_(*this); }

int Http2Session::downstream_read(const uint8_t *data, size_t datalen) {
  auto rv = nghttp2_session_mem_recv2(session_, data, datalen);
  if (rv < 0) {
    SSLOG(ERROR, this) << "nghttp2_session_mem_recv2() returned error: "
                       << nghttp2_strerror(rv);
    return -1;
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wb_.rleft() == 0) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "No more read/write for this HTTP2 session";
    }
    return -1;
  }

  signal_write();
  return 0;
}

int Http2Session::downstream_write() {
  for (;;) {
    const uint8_t *data;
    auto datalen = nghttp2_session_mem_send2(session_, &data);
    if (datalen < 0) {
      SSLOG(ERROR, this) << "nghttp2_session_mem_send2() returned error: "
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
      SSLOG(INFO, this) << "No more read/write for this session";
    }
    return -1;
  }

  return 0;
}

void Http2Session::signal_write() {
  switch (state_) {
  case Http2SessionState::DISCONNECTED:
    if (!ev_is_active(&initiate_connection_timer_)) {
      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "Start connecting to backend server";
      }
      // Since the timer is set to 0., these will feed 2 events.  We
      // will stop the timer in the initiate_connection_timer_ to void
      // 2nd event.
      ev_timer_start(conn_.loop, &initiate_connection_timer_);
      ev_feed_event(conn_.loop, &initiate_connection_timer_, 0);
    }
    break;
  case Http2SessionState::CONNECTED:
    conn_.wlimit.startw();
    break;
  default:
    break;
  }
}

struct ev_loop *Http2Session::get_loop() const { return conn_.loop; }

ev_io *Http2Session::get_wev() { return &conn_.wev; }

Http2SessionState Http2Session::get_state() const { return state_; }

void Http2Session::set_state(Http2SessionState state) { state_ = state; }

int Http2Session::terminate_session(uint32_t error_code) {
  int rv;
  rv = nghttp2_session_terminate_session(session_, error_code);
  if (rv != 0) {
    return -1;
  }
  return 0;
}

SSL *Http2Session::get_ssl() const { return conn_.tls.ssl; }

int Http2Session::consume(int32_t stream_id, size_t len) {
  int rv;

  if (!session_) {
    return 0;
  }

  rv = nghttp2_session_consume(session_, stream_id, len);

  if (rv != 0) {
    SSLOG(WARN, this) << "nghttp2_session_consume() returned error: "
                      << nghttp2_strerror(rv);

    return -1;
  }

  return 0;
}

bool Http2Session::can_push_request(const Downstream *downstream) const {
  auto &req = downstream->request();
  return state_ == Http2SessionState::CONNECTED &&
         connection_check_state_ == ConnectionCheck::NONE &&
         (req.connect_proto == ConnectProto::NONE || settings_recved_);
}

void Http2Session::start_checking_connection() {
  if (state_ != Http2SessionState::CONNECTED ||
      connection_check_state_ != ConnectionCheck::REQUIRED) {
    return;
  }
  connection_check_state_ = ConnectionCheck::STARTED;

  SSLOG(INFO, this) << "Start checking connection";
  // If connection is down, we may get error when writing data.  Issue
  // ping frame to see whether connection is alive.
  nghttp2_submit_ping(session_, NGHTTP2_FLAG_NONE, nullptr);

  // set ping timeout and start timer again
  reset_connection_check_timer(CONNCHK_PING_TIMEOUT);

  signal_write();
}

void Http2Session::reset_connection_check_timer(ev_tstamp t) {
  connchk_timer_.repeat = t;
  ev_timer_again(conn_.loop, &connchk_timer_);
}

void Http2Session::reset_connection_check_timer_if_not_checking() {
  if (connection_check_state_ != ConnectionCheck::NONE) {
    return;
  }

  reset_connection_check_timer(CONNCHK_TIMEOUT);
}

void Http2Session::connection_alive() {
  reset_connection_check_timer(CONNCHK_TIMEOUT);

  if (connection_check_state_ == ConnectionCheck::NONE) {
    return;
  }

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Connection alive";
  }

  connection_check_state_ = ConnectionCheck::NONE;

  submit_pending_requests();
}

void Http2Session::submit_pending_requests() {
  for (auto dconn = dconns_.head; dconn; dconn = dconn->dlnext) {
    auto downstream = dconn->get_downstream();

    if (!downstream->get_request_pending() ||
        !downstream->request_submission_ready()) {
      continue;
    }

    auto &req = downstream->request();
    if (req.connect_proto != ConnectProto::NONE && !settings_recved_) {
      continue;
    }

    auto upstream = downstream->get_upstream();

    if (dconn->push_request_headers() != 0) {
      if (LOG_ENABLED(INFO)) {
        SSLOG(INFO, this) << "backend request failed";
      }

      upstream->on_downstream_abort_request(downstream, 400);

      continue;
    }

    upstream->resume_read(SHRPX_NO_BUFFER, downstream, 0);
  }
}

void Http2Session::set_connection_check_state(ConnectionCheck state) {
  connection_check_state_ = state;
}

ConnectionCheck Http2Session::get_connection_check_state() const {
  return connection_check_state_;
}

int Http2Session::noop() { return 0; }

int Http2Session::read_noop(const uint8_t *data, size_t datalen) { return 0; }

int Http2Session::write_noop() { return 0; }

int Http2Session::connected() {
  auto sock_error = util::get_socket_error(conn_.fd);
  if (sock_error != 0) {
    SSLOG(WARN, this) << "Backend connect failed; addr="
                      << util::to_numeric_addr(raddr_)
                      << ": errno=" << sock_error;

    downstream_failure(addr_, raddr_);

    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Connection established";
  }

  // Reset timeout for write.  Previously, we set timeout for connect.
  conn_.wt.repeat = group_->shared_addr->timeout.write;
  ev_timer_again(conn_.loop, &conn_.wt);

  conn_.rlimit.startw();
  conn_.again_rt();

  read_ = &Http2Session::read_clear;
  write_ = &Http2Session::write_clear;

  if (state_ == Http2SessionState::PROXY_CONNECTING) {
    return do_write();
  }

  if (conn_.tls.ssl) {
    read_ = &Http2Session::tls_handshake;
    write_ = &Http2Session::tls_handshake;

    return do_write();
  }

  if (connection_made() != 0) {
    state_ = Http2SessionState::CONNECT_FAILING;
    return -1;
  }

  return 0;
}

int Http2Session::read_clear() {
  conn_.last_read = std::chrono::steady_clock::now();

  std::array<uint8_t, 16_k> buf;

  for (;;) {
    auto nread = conn_.read_clear(buf.data(), buf.size());

    if (nread == 0) {
      return write_clear();
    }

    if (nread < 0) {
      return nread;
    }

    if (on_read(buf.data(), nread) != 0) {
      return -1;
    }
  }
}

int Http2Session::write_clear() {
  conn_.last_read = std::chrono::steady_clock::now();

  std::array<struct iovec, MAX_WR_IOVCNT> iov;

  for (;;) {
    if (wb_.rleft() > 0) {
      auto iovcnt = wb_.riovec(iov.data(), iov.size());
      auto nwrite = conn_.writev_clear(iov.data(), iovcnt);

      if (nwrite == 0) {
        return 0;
      }

      if (nwrite < 0) {
        // We may have pending data in receive buffer which may
        // contain part of response body.  So keep reading.  Invoke
        // read event to get read(2) error just in case.
        ev_feed_event(conn_.loop, &conn_.rev, EV_READ);
        write_ = &Http2Session::write_void;
        break;
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

  return 0;
}

int Http2Session::tls_handshake() {
  conn_.last_read = std::chrono::steady_clock::now();

  ERR_clear_error();

  auto rv = conn_.tls_handshake();

  if (rv == SHRPX_ERR_INPROGRESS) {
    return 0;
  }

  if (rv < 0) {
    downstream_failure(addr_, raddr_);

    return rv;
  }

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "SSL/TLS handshake completed";
  }

  if (!get_config()->tls.insecure &&
      tls::check_cert(conn_.tls.ssl, addr_, raddr_) != 0) {
    downstream_failure(addr_, raddr_);

    return -1;
  }

  read_ = &Http2Session::read_tls;
  write_ = &Http2Session::write_tls;

  if (connection_made() != 0) {
    state_ = Http2SessionState::CONNECT_FAILING;
    return -1;
  }

  return 0;
}

int Http2Session::read_tls() {
  conn_.last_read = std::chrono::steady_clock::now();

  std::array<uint8_t, 16_k> buf;

  ERR_clear_error();

  for (;;) {
    auto nread = conn_.read_tls(buf.data(), buf.size());

    if (nread == 0) {
      return write_tls();
    }

    if (nread < 0) {
      return nread;
    }

    if (on_read(buf.data(), nread) != 0) {
      return -1;
    }
  }
}

int Http2Session::write_tls() {
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
        // We may have pending data in receive buffer which may
        // contain part of response body.  So keep reading.  Invoke
        // read event to get read(2) error just in case.
        ev_feed_event(conn_.loop, &conn_.rev, EV_READ);
        write_ = &Http2Session::write_void;
        break;
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

  return 0;
}

int Http2Session::write_void() {
  conn_.wlimit.stopw();
  return 0;
}

bool Http2Session::should_hard_fail() const {
  switch (state_) {
  case Http2SessionState::PROXY_CONNECTING:
  case Http2SessionState::PROXY_FAILED:
    return true;
  case Http2SessionState::DISCONNECTED: {
    const auto &proxy = get_config()->downstream_http_proxy;
    return !proxy.host.empty();
  }
  default:
    return false;
  }
}

DownstreamAddr *Http2Session::get_addr() const { return addr_; }

int Http2Session::handle_downstream_push_promise(Downstream *downstream,
                                                 int32_t promised_stream_id) {
  auto upstream = downstream->get_upstream();
  if (!upstream->push_enabled()) {
    return -1;
  }

  auto promised_downstream =
    upstream->on_downstream_push_promise(downstream, promised_stream_id);
  if (!promised_downstream) {
    return -1;
  }

  // Now we have Downstream object for pushed stream.
  // promised_downstream->get_stream() still returns 0.

  auto handler = upstream->get_client_handler();

  auto promised_dconn = std::make_unique<Http2DownstreamConnection>(this);
  promised_dconn->set_client_handler(handler);

  auto ptr = promised_dconn.get();

  if (promised_downstream->attach_downstream_connection(
        std::move(promised_dconn)) != 0) {
    return -1;
  }

  auto promised_sd = std::make_unique<StreamData>();

  nghttp2_session_set_stream_user_data(session_, promised_stream_id,
                                       promised_sd.get());

  ptr->attach_stream_data(promised_sd.get());
  streams_.append(promised_sd.release());

  return 0;
}

int Http2Session::handle_downstream_push_promise_complete(
  Downstream *downstream, Downstream *promised_downstream) {
  auto &promised_req = promised_downstream->request();

  auto &promised_balloc = promised_downstream->get_block_allocator();

  auto authority = promised_req.fs.header(http2::HD__AUTHORITY);
  auto path = promised_req.fs.header(http2::HD__PATH);
  auto method = promised_req.fs.header(http2::HD__METHOD);
  auto scheme = promised_req.fs.header(http2::HD__SCHEME);

  if (!authority) {
    authority = promised_req.fs.header(http2::HD_HOST);
  }

  auto method_token = http2::lookup_method_token(method->value);
  if (method_token == -1) {
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "Unrecognized method: " << method->value;
    }

    return -1;
  }

  // TODO Rewrite authority if we enabled rewrite host.  But we
  // really don't know how to rewrite host.  Should we use the same
  // host in associated stream?
  if (authority) {
    promised_req.authority = authority->value;
  }
  promised_req.method = method_token;
  // libnghttp2 ensures that we don't have CONNECT method in
  // PUSH_PROMISE, and guarantees that :scheme exists.
  if (scheme) {
    promised_req.scheme = scheme->value;
  }

  // For server-wide OPTIONS request, path is empty.
  if (method_token != HTTP_OPTIONS || path->value != "*"_sr) {
    promised_req.path = http2::rewrite_clean_path(promised_balloc, path->value);
  }

  promised_downstream->inspect_http2_request();

  auto upstream = promised_downstream->get_upstream();

  promised_downstream->set_request_state(DownstreamState::MSG_COMPLETE);
  promised_downstream->set_request_header_sent(true);

  if (upstream->on_downstream_push_promise_complete(downstream,
                                                    promised_downstream) != 0) {
    return -1;
  }

  return 0;
}

size_t Http2Session::get_num_dconns() const { return dconns_.size(); }

bool Http2Session::max_concurrency_reached(size_t extra) const {
  if (!session_) {
    return dconns_.size() + extra >= 100;
  }

  // If session does not allow further requests, it effectively means
  // that maximum concurrency is reached.
  return !nghttp2_session_check_request_allowed(session_) ||
         dconns_.size() + extra >=
           nghttp2_session_get_remote_settings(
             session_, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
}

const std::shared_ptr<DownstreamAddrGroup> &
Http2Session::get_downstream_addr_group() const {
  return group_;
}

void Http2Session::add_to_extra_freelist() {
  if (freelist_zone_ != FreelistZone::NONE) {
    return;
  }

  if (LOG_ENABLED(INFO)) {
    SSLOG(INFO, this) << "Append to http2_extra_freelist, addr=" << addr_
                      << ", freelist.size="
                      << addr_->http2_extra_freelist.size();
  }

  freelist_zone_ = FreelistZone::EXTRA;
  addr_->http2_extra_freelist.append(this);
}

void Http2Session::remove_from_freelist() {
  switch (freelist_zone_) {
  case FreelistZone::NONE:
    return;
  case FreelistZone::EXTRA:
    if (LOG_ENABLED(INFO)) {
      SSLOG(INFO, this) << "Remove from http2_extra_freelist, addr=" << addr_
                        << ", freelist.size="
                        << addr_->http2_extra_freelist.size();
    }
    addr_->http2_extra_freelist.remove(this);
    break;
  case FreelistZone::GONE:
    return;
  }

  freelist_zone_ = FreelistZone::NONE;
}

void Http2Session::exclude_from_scheduling() {
  remove_from_freelist();
  freelist_zone_ = FreelistZone::GONE;
}

DefaultMemchunks *Http2Session::get_request_buf() { return &wb_; }

void Http2Session::on_timeout() {
  switch (state_) {
  case Http2SessionState::PROXY_CONNECTING: {
    auto worker_blocker = worker_->get_connect_blocker();
    worker_blocker->on_failure();
    break;
  }
  case Http2SessionState::CONNECTING:
    SSLOG(WARN, this) << "Connect time out; addr="
                      << util::to_numeric_addr(raddr_);

    downstream_failure(addr_, raddr_);
    break;
  default:
    break;
  }
}

void Http2Session::check_retire() {
  if (!group_->retired) {
    return;
  }

  ev_prepare_stop(conn_.loop, &prep_);

  if (!session_) {
    return;
  }

  auto last_stream_id = nghttp2_session_get_last_proc_stream_id(session_);
  nghttp2_submit_goaway(session_, NGHTTP2_FLAG_NONE, last_stream_id,
                        NGHTTP2_NO_ERROR, nullptr, 0);

  signal_write();
}

const Address *Http2Session::get_raddr() const { return raddr_; }

void Http2Session::on_settings_received(const nghttp2_frame *frame) {
  // TODO This effectively disallows nghttpx to change its behaviour
  // based on the 2nd SETTINGS.
  if (settings_recved_) {
    return;
  }

  settings_recved_ = true;

  for (size_t i = 0; i < frame->settings.niv; ++i) {
    auto &ent = frame->settings.iv[i];
    if (ent.settings_id == NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL) {
      allow_connect_proto_ = true;
      break;
    }
  }

  submit_pending_requests();
}

bool Http2Session::get_allow_connect_proto() const {
  return allow_connect_proto_;
}

} // namespace shrpx
