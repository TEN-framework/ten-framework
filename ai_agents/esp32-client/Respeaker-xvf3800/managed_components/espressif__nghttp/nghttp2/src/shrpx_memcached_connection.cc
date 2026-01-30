/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#include "shrpx_memcached_connection.h"

#include <limits.h>
#include <sys/uio.h>

#include <cerrno>

#include "shrpx_memcached_request.h"
#include "shrpx_memcached_result.h"
#include "shrpx_config.h"
#include "shrpx_tls.h"
#include "shrpx_log.h"
#include "util.h"

namespace shrpx {

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto mconn = static_cast<MemcachedConnection *>(conn->data);

  if (w == &conn->rt && !conn->expired_rt()) {
    return;
  }

  if (LOG_ENABLED(INFO)) {
    MCLOG(INFO, mconn) << "Time out";
  }

  mconn->disconnect();
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto mconn = static_cast<MemcachedConnection *>(conn->data);

  if (mconn->on_read() != 0) {
    mconn->reconnect_or_fail();
    return;
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto mconn = static_cast<MemcachedConnection *>(conn->data);

  if (mconn->on_write() != 0) {
    mconn->reconnect_or_fail();
    return;
  }
}
} // namespace

namespace {
void connectcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto mconn = static_cast<MemcachedConnection *>(conn->data);

  if (mconn->connected() != 0) {
    mconn->disconnect();
    return;
  }

  writecb(loop, w, revents);
}
} // namespace

constexpr auto write_timeout = 10_s;
constexpr auto read_timeout = 10_s;

MemcachedConnection::MemcachedConnection(const Address *addr,
                                         struct ev_loop *loop, SSL_CTX *ssl_ctx,
                                         const StringRef &sni_name,
                                         MemchunkPool *mcpool,
                                         std::mt19937 &gen)
  : conn_(loop, -1, nullptr, mcpool, write_timeout, read_timeout, {}, {},
          connectcb, readcb, timeoutcb, this, 0, 0., Proto::MEMCACHED),
    do_read_(&MemcachedConnection::noop),
    do_write_(&MemcachedConnection::noop),
    sni_name_(sni_name),
    connect_blocker_(
      gen, loop, [] {}, [] {}),
    parse_state_{},
    addr_(addr),
    ssl_ctx_(ssl_ctx),
    sendsum_(0),
    try_count_(0),
    connected_(false) {}

MemcachedConnection::~MemcachedConnection() { conn_.disconnect(); }

namespace {
void clear_request(std::deque<std::unique_ptr<MemcachedRequest>> &q) {
  for (auto &req : q) {
    if (req->cb) {
      req->cb(req.get(),
              MemcachedResult(MemcachedStatusCode::EXT_NETWORK_ERROR));
    }
  }
  q.clear();
}
} // namespace

void MemcachedConnection::disconnect() {
  clear_request(recvq_);
  clear_request(sendq_);

  sendbufv_.clear();
  sendsum_ = 0;

  parse_state_ = {};

  connected_ = false;

  conn_.disconnect();

  assert(recvbuf_.rleft() == 0);
  recvbuf_.reset();

  do_read_ = do_write_ = &MemcachedConnection::noop;
}

int MemcachedConnection::initiate_connection() {
  assert(conn_.fd == -1);

  if (ssl_ctx_) {
    auto ssl = tls::create_ssl(ssl_ctx_);
    if (!ssl) {
      return -1;
    }
    conn_.set_ssl(ssl);
    conn_.tls.client_session_cache = &tls_session_cache_;
  }

  conn_.fd = util::create_nonblock_socket(addr_->su.storage.ss_family);

  if (conn_.fd == -1) {
    auto error = errno;
    MCLOG(WARN, this) << "socket() failed; errno=" << error;

    return -1;
  }

  int rv;
  rv = connect(conn_.fd, &addr_->su.sa, addr_->len);
  if (rv != 0 && errno != EINPROGRESS) {
    auto error = errno;
    MCLOG(WARN, this) << "connect() failed; errno=" << error;

    close(conn_.fd);
    conn_.fd = -1;

    return -1;
  }

  if (ssl_ctx_) {
    if (!util::numeric_host(sni_name_.data())) {
      SSL_set_tlsext_host_name(conn_.tls.ssl, sni_name_.data());
    }

    auto session = tls::reuse_tls_session(tls_session_cache_);
    if (session) {
      SSL_set_session(conn_.tls.ssl, session);
      SSL_SESSION_free(session);
    }

    conn_.prepare_client_handshake();
  }

  if (LOG_ENABLED(INFO)) {
    MCLOG(INFO, this) << "Connecting to memcached server";
  }

  ev_io_set(&conn_.wev, conn_.fd, EV_WRITE);
  ev_io_set(&conn_.rev, conn_.fd, EV_READ);

  ev_set_cb(&conn_.wev, connectcb);

  conn_.wlimit.startw();
  ev_timer_again(conn_.loop, &conn_.wt);

  return 0;
}

int MemcachedConnection::connected() {
  auto sock_error = util::get_socket_error(conn_.fd);
  if (sock_error != 0) {
    MCLOG(WARN, this) << "memcached connect failed; addr="
                      << util::to_numeric_addr(addr_)
                      << ": errno=" << sock_error;

    connect_blocker_.on_failure();

    conn_.wlimit.stopw();

    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    MCLOG(INFO, this) << "connected to memcached server";
  }

  conn_.rlimit.startw();

  ev_set_cb(&conn_.wev, writecb);

  if (conn_.tls.ssl) {
    conn_.again_rt();

    do_read_ = &MemcachedConnection::tls_handshake;
    do_write_ = &MemcachedConnection::tls_handshake;

    return 0;
  }

  ev_timer_stop(conn_.loop, &conn_.wt);

  connected_ = true;

  connect_blocker_.on_success();

  do_read_ = &MemcachedConnection::read_clear;
  do_write_ = &MemcachedConnection::write_clear;

  return 0;
}

int MemcachedConnection::on_write() { return do_write_(*this); }
int MemcachedConnection::on_read() { return do_read_(*this); }

int MemcachedConnection::tls_handshake() {
  ERR_clear_error();

  conn_.last_read = std::chrono::steady_clock::now();

  auto rv = conn_.tls_handshake();
  if (rv == SHRPX_ERR_INPROGRESS) {
    return 0;
  }

  if (rv < 0) {
    connect_blocker_.on_failure();
    return rv;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "SSL/TLS handshake completed";
  }

  auto &tlsconf = get_config()->tls;

  if (!tlsconf.insecure &&
      tls::check_cert(conn_.tls.ssl, addr_, sni_name_) != 0) {
    connect_blocker_.on_failure();
    return -1;
  }

  ev_timer_stop(conn_.loop, &conn_.rt);
  ev_timer_stop(conn_.loop, &conn_.wt);

  connected_ = true;

  connect_blocker_.on_success();

  do_read_ = &MemcachedConnection::read_tls;
  do_write_ = &MemcachedConnection::write_tls;

  return on_write();
}

int MemcachedConnection::write_tls() {
  if (!connected_) {
    return 0;
  }

  conn_.last_read = std::chrono::steady_clock::now();

  std::array<struct iovec, MAX_WR_IOVCNT> iov;
  std::array<uint8_t, 16_k> buf;

  for (; !sendq_.empty();) {
    auto iovcnt = fill_request_buffer(iov.data(), iov.size());
    auto p = std::begin(buf);
    for (size_t i = 0; i < iovcnt; ++i) {
      auto &v = iov[i];
      auto n = std::min(static_cast<size_t>(std::end(buf) - p), v.iov_len);
      p = std::copy_n(static_cast<uint8_t *>(v.iov_base), n, p);
      if (p == std::end(buf)) {
        break;
      }
    }

    auto nwrite = conn_.write_tls(buf.data(), p - std::begin(buf));
    if (nwrite < 0) {
      return -1;
    }
    if (nwrite == 0) {
      return 0;
    }

    drain_send_queue(nwrite);
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  return 0;
}

int MemcachedConnection::read_tls() {
  if (!connected_) {
    return 0;
  }

  conn_.last_read = std::chrono::steady_clock::now();

  for (;;) {
    auto nread = conn_.read_tls(recvbuf_.last, recvbuf_.wleft());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    recvbuf_.write(nread);

    if (parse_packet() != 0) {
      return -1;
    }
  }

  return 0;
}

int MemcachedConnection::write_clear() {
  if (!connected_) {
    return 0;
  }

  conn_.last_read = std::chrono::steady_clock::now();

  std::array<struct iovec, MAX_WR_IOVCNT> iov;

  for (; !sendq_.empty();) {
    auto iovcnt = fill_request_buffer(iov.data(), iov.size());
    auto nwrite = conn_.writev_clear(iov.data(), iovcnt);
    if (nwrite < 0) {
      return -1;
    }
    if (nwrite == 0) {
      return 0;
    }

    drain_send_queue(nwrite);
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  return 0;
}

int MemcachedConnection::read_clear() {
  if (!connected_) {
    return 0;
  }

  conn_.last_read = std::chrono::steady_clock::now();

  for (;;) {
    auto nread = conn_.read_clear(recvbuf_.last, recvbuf_.wleft());

    if (nread == 0) {
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    recvbuf_.write(nread);

    if (parse_packet() != 0) {
      return -1;
    }
  }

  return 0;
}

int MemcachedConnection::parse_packet() {
  auto in = recvbuf_.pos;

  for (;;) {
    auto busy = false;

    switch (parse_state_.state) {
    case MemcachedParseState::HEADER24: {
      if (recvbuf_.last - in < 24) {
        recvbuf_.drain_reset(in - recvbuf_.pos);
        return 0;
      }

      if (recvq_.empty()) {
        MCLOG(WARN, this)
          << "Response received, but there is no in-flight request.";
        return -1;
      }

      auto &req = recvq_.front();

      if (*in != MEMCACHED_RES_MAGIC) {
        MCLOG(WARN, this) << "Response has bad magic: "
                          << static_cast<uint32_t>(*in);
        return -1;
      }
      ++in;

      parse_state_.op = static_cast<MemcachedOp>(*in++);
      parse_state_.keylen = util::get_uint16(in);
      in += 2;
      parse_state_.extralen = *in++;
      // skip 1 byte reserved data type
      ++in;
      parse_state_.status_code =
        static_cast<MemcachedStatusCode>(util::get_uint16(in));
      in += 2;
      parse_state_.totalbody = util::get_uint32(in);
      in += 4;
      // skip 4 bytes opaque
      in += 4;
      parse_state_.cas = util::get_uint64(in);
      in += 8;

      if (req->op != parse_state_.op) {
        MCLOG(WARN, this)
          << "opcode in response does not match to the request: want "
          << static_cast<uint32_t>(req->op) << ", got "
          << static_cast<uint32_t>(parse_state_.op);
        return -1;
      }

      if (parse_state_.keylen != 0) {
        MCLOG(WARN, this) << "zero length keylen expected: got "
                          << parse_state_.keylen;
        return -1;
      }

      if (parse_state_.totalbody > 16_k) {
        MCLOG(WARN, this) << "totalbody is too large: got "
                          << parse_state_.totalbody;
        return -1;
      }

      if (parse_state_.op == MemcachedOp::GET &&
          parse_state_.status_code == MemcachedStatusCode::NO_ERROR &&
          parse_state_.extralen == 0) {
        MCLOG(WARN, this) << "response for GET does not have extra";
        return -1;
      }

      if (parse_state_.totalbody <
          parse_state_.keylen + parse_state_.extralen) {
        MCLOG(WARN, this) << "totalbody is too short: totalbody "
                          << parse_state_.totalbody << ", want min "
                          << parse_state_.keylen + parse_state_.extralen;
        return -1;
      }

      if (parse_state_.extralen) {
        parse_state_.state = MemcachedParseState::EXTRA;
        parse_state_.read_left = parse_state_.extralen;
      } else {
        parse_state_.state = MemcachedParseState::VALUE;
        parse_state_.read_left =
          parse_state_.totalbody - parse_state_.keylen - parse_state_.extralen;
      }
      busy = true;
      break;
    }
    case MemcachedParseState::EXTRA: {
      // We don't use extra for now. Just read and forget.
      auto n = std::min(static_cast<size_t>(recvbuf_.last - in),
                        parse_state_.read_left);

      parse_state_.read_left -= n;
      in += n;
      if (parse_state_.read_left) {
        recvbuf_.reset();
        return 0;
      }
      parse_state_.state = MemcachedParseState::VALUE;
      // since we require keylen == 0, totalbody - extralen ==
      // valuelen
      parse_state_.read_left =
        parse_state_.totalbody - parse_state_.keylen - parse_state_.extralen;
      busy = true;
      break;
    }
    case MemcachedParseState::VALUE: {
      auto n = std::min(static_cast<size_t>(recvbuf_.last - in),
                        parse_state_.read_left);

      parse_state_.value.insert(std::end(parse_state_.value), in, in + n);

      parse_state_.read_left -= n;
      in += n;
      if (parse_state_.read_left) {
        recvbuf_.reset();
        return 0;
      }

      if (LOG_ENABLED(INFO)) {
        if (parse_state_.status_code != MemcachedStatusCode::NO_ERROR) {
          MCLOG(INFO, this) << "response returned error status: "
                            << static_cast<uint16_t>(parse_state_.status_code);
        }
      }

      // We require at least one complete response to clear try count.
      try_count_ = 0;

      auto req = std::move(recvq_.front());
      recvq_.pop_front();

      if (sendq_.empty() && recvq_.empty()) {
        ev_timer_stop(conn_.loop, &conn_.rt);
      }

      if (!req->canceled && req->cb) {
        req->cb(req.get(), MemcachedResult(parse_state_.status_code,
                                           std::move(parse_state_.value)));
      }

      parse_state_ = {};
      break;
    }
    }

    if (!busy && in == recvbuf_.last) {
      break;
    }
  }

  assert(in == recvbuf_.last);
  recvbuf_.reset();

  return 0;
}

#undef DEFAULT_WR_IOVCNT
#define DEFAULT_WR_IOVCNT 128

#if defined(IOV_MAX) && IOV_MAX < DEFAULT_WR_IOVCNT
#  define MAX_WR_IOVCNT IOV_MAX
#else // !defined(IOV_MAX) || IOV_MAX >= DEFAULT_WR_IOVCNT
#  define MAX_WR_IOVCNT DEFAULT_WR_IOVCNT
#endif // !defined(IOV_MAX) || IOV_MAX >= DEFAULT_WR_IOVCNT

size_t MemcachedConnection::fill_request_buffer(struct iovec *iov,
                                                size_t iovlen) {
  if (sendsum_ == 0) {
    for (auto &req : sendq_) {
      if (req->canceled) {
        continue;
      }
      if (serialized_size(req.get()) + sendsum_ > 1300) {
        break;
      }
      sendbufv_.emplace_back();
      sendbufv_.back().req = req.get();
      make_request(&sendbufv_.back(), req.get());
      sendsum_ += sendbufv_.back().left();
    }

    if (sendsum_ == 0) {
      sendq_.clear();
      return 0;
    }
  }

  size_t iovcnt = 0;
  for (auto &buf : sendbufv_) {
    if (iovcnt + 2 > iovlen) {
      break;
    }

    auto req = buf.req;
    if (buf.headbuf.rleft()) {
      iov[iovcnt++] = {buf.headbuf.pos, buf.headbuf.rleft()};
    }
    if (buf.send_value_left) {
      iov[iovcnt++] = {req->value.data() + req->value.size() -
                         buf.send_value_left,
                       buf.send_value_left};
    }
  }

  return iovcnt;
}

void MemcachedConnection::drain_send_queue(size_t nwrite) {
  sendsum_ -= nwrite;

  while (nwrite > 0) {
    auto &buf = sendbufv_.front();
    auto &req = sendq_.front();
    if (req->canceled) {
      sendq_.pop_front();
      continue;
    }
    assert(buf.req == req.get());
    auto n = std::min(static_cast<size_t>(nwrite), buf.headbuf.rleft());
    buf.headbuf.drain(n);
    nwrite -= n;
    n = std::min(static_cast<size_t>(nwrite), buf.send_value_left);
    buf.send_value_left -= n;
    nwrite -= n;

    if (buf.headbuf.rleft() || buf.send_value_left) {
      break;
    }
    sendbufv_.pop_front();
    recvq_.push_back(std::move(sendq_.front()));
    sendq_.pop_front();
  }

  // start read timer only when we wait for responses.
  if (recvq_.empty()) {
    ev_timer_stop(conn_.loop, &conn_.rt);
  } else if (!ev_is_active(&conn_.rt)) {
    conn_.again_rt();
  }
}

size_t MemcachedConnection::serialized_size(MemcachedRequest *req) {
  switch (req->op) {
  case MemcachedOp::GET:
    return 24 + req->key.size();
  case MemcachedOp::ADD:
  default:
    return 24 + 8 + req->key.size() + req->value.size();
  }
}

void MemcachedConnection::make_request(MemcachedSendbuf *sendbuf,
                                       MemcachedRequest *req) {
  auto &headbuf = sendbuf->headbuf;

  std::fill(std::begin(headbuf.buf), std::end(headbuf.buf), 0);

  headbuf[0] = MEMCACHED_REQ_MAGIC;
  headbuf[1] = static_cast<uint8_t>(req->op);
  switch (req->op) {
  case MemcachedOp::GET:
    util::put_uint16be(&headbuf[2], req->key.size());
    util::put_uint32be(&headbuf[8], req->key.size());
    headbuf.write(24);
    break;
  case MemcachedOp::ADD:
    util::put_uint16be(&headbuf[2], req->key.size());
    headbuf[4] = 8;
    util::put_uint32be(&headbuf[8], 8 + req->key.size() + req->value.size());
    util::put_uint32be(&headbuf[28], req->expiry);
    headbuf.write(32);
    break;
  }

  headbuf.write(req->key.c_str(), req->key.size());

  sendbuf->send_value_left = req->value.size();
}

int MemcachedConnection::add_request(std::unique_ptr<MemcachedRequest> req) {
  if (connect_blocker_.blocked()) {
    return -1;
  }

  sendq_.push_back(std::move(req));

  if (connected_) {
    signal_write();
    return 0;
  }

  if (conn_.fd == -1 && initiate_connection() != 0) {
    connect_blocker_.on_failure();
    disconnect();
    return -1;
  }

  return 0;
}

// TODO should we start write timer too?
void MemcachedConnection::signal_write() { conn_.wlimit.startw(); }

int MemcachedConnection::noop() { return 0; }

void MemcachedConnection::reconnect_or_fail() {
  if (!connected_ || (recvq_.empty() && sendq_.empty())) {
    disconnect();
    return;
  }

  constexpr size_t MAX_TRY_COUNT = 3;

  if (++try_count_ >= MAX_TRY_COUNT) {
    if (LOG_ENABLED(INFO)) {
      MCLOG(INFO, this) << "Tried " << MAX_TRY_COUNT
                        << " times, and all failed.  Aborting";
    }
    try_count_ = 0;
    disconnect();
    return;
  }

  std::vector<std::unique_ptr<MemcachedRequest>> q;
  q.reserve(recvq_.size() + sendq_.size());

  if (LOG_ENABLED(INFO)) {
    MCLOG(INFO, this) << "Retry connection, enqueue "
                      << recvq_.size() + sendq_.size() << " request(s) again";
  }

  q.insert(std::end(q), std::make_move_iterator(std::begin(recvq_)),
           std::make_move_iterator(std::end(recvq_)));
  q.insert(std::end(q), std::make_move_iterator(std::begin(sendq_)),
           std::make_move_iterator(std::end(sendq_)));

  recvq_.clear();
  sendq_.clear();

  disconnect();

  sendq_.insert(std::end(sendq_), std::make_move_iterator(std::begin(q)),
                std::make_move_iterator(std::end(q)));

  if (initiate_connection() != 0) {
    connect_blocker_.on_failure();
    disconnect();
    return;
  }
}

} // namespace shrpx
