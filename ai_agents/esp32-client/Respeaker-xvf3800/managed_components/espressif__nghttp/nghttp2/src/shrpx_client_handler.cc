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
#include "shrpx_client_handler.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H

#include <cerrno>

#include "shrpx_upstream.h"
#include "shrpx_http2_upstream.h"
#include "shrpx_https_upstream.h"
#include "shrpx_config.h"
#include "shrpx_http_downstream_connection.h"
#include "shrpx_http2_downstream_connection.h"
#include "shrpx_tls.h"
#include "shrpx_worker.h"
#include "shrpx_downstream_connection_pool.h"
#include "shrpx_downstream.h"
#include "shrpx_http2_session.h"
#include "shrpx_connect_blocker.h"
#include "shrpx_api_downstream_connection.h"
#include "shrpx_health_monitor_downstream_connection.h"
#include "shrpx_null_downstream_connection.h"
#ifdef ENABLE_HTTP3
#  include "shrpx_http3_upstream.h"
#endif // ENABLE_HTTP3
#include "shrpx_log.h"
#include "util.h"
#include "template.h"
#include "tls.h"

using namespace nghttp2;

namespace shrpx {

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, handler) << "Time out";
  }

  delete handler;
}
} // namespace

namespace {
void shutdowncb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto handler = static_cast<ClientHandler *>(w->data);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, handler) << "Close connection due to TLS renegotiation";
  }

  delete handler;
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (handler->do_read() != 0) {
    delete handler;
    return;
  }
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto conn = static_cast<Connection *>(w->data);
  auto handler = static_cast<ClientHandler *>(conn->data);

  if (handler->do_write() != 0) {
    delete handler;
    return;
  }
}
} // namespace

int ClientHandler::noop() { return 0; }

int ClientHandler::read_clear() {
  auto should_break = false;
  rb_.ensure_chunk();
  for (;;) {
    if (rb_.rleft() && on_read() != 0) {
      return -1;
    }
    if (rb_.rleft() == 0) {
      rb_.reset();
    } else if (rb_.wleft() == 0) {
      conn_.rlimit.stopw();
      return 0;
    }

    if (!ev_is_active(&conn_.rev) || should_break) {
      return 0;
    }

    auto nread = conn_.read_clear(rb_.last(), rb_.wleft());

    if (nread == 0) {
      if (rb_.rleft() == 0) {
        rb_.release_chunk();
      }
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    rb_.write(nread);
    should_break = true;
  }
}

int ClientHandler::write_clear() {
  std::array<iovec, 2> iov;

  for (;;) {
    if (on_write() != 0) {
      return -1;
    }

    auto iovcnt = upstream_->response_riovec(iov.data(), iov.size());
    if (iovcnt == 0) {
      break;
    }

    auto nwrite = conn_.writev_clear(iov.data(), iovcnt);
    if (nwrite < 0) {
      return -1;
    }

    if (nwrite == 0) {
      return 0;
    }

    upstream_->response_drain(nwrite);
  }

  conn_.wlimit.stopw();
  ev_timer_stop(conn_.loop, &conn_.wt);

  return 0;
}

int ClientHandler::proxy_protocol_peek_clear() {
  rb_.ensure_chunk();

  assert(rb_.rleft() == 0);

  auto nread = conn_.peek_clear(rb_.last(), rb_.wleft());
  if (nread < 0) {
    return -1;
  }
  if (nread == 0) {
    return 0;
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol: Peek " << nread
                     << " bytes from socket";
  }

  rb_.write(nread);

  if (on_read() != 0) {
    return -1;
  }

  rb_.reset();

  return 0;
}

int ClientHandler::tls_handshake() {
  ev_timer_again(conn_.loop, &conn_.rt);

  ERR_clear_error();

  auto rv = conn_.tls_handshake();

  if (rv == SHRPX_ERR_INPROGRESS) {
    return 0;
  }

  if (rv < 0) {
    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "SSL/TLS handshake completed";
  }

  if (validate_next_proto() != 0) {
    return -1;
  }

  read_ = &ClientHandler::read_tls;
  write_ = &ClientHandler::write_tls;

  return 0;
}

int ClientHandler::read_tls() {
  auto should_break = false;

  ERR_clear_error();

  rb_.ensure_chunk();

  for (;;) {
    // we should process buffered data first before we read EOF.
    if (rb_.rleft() && on_read() != 0) {
      return -1;
    }
    if (rb_.rleft() == 0) {
      rb_.reset();
    } else if (rb_.wleft() == 0) {
      conn_.rlimit.stopw();
      return 0;
    }

    if (!ev_is_active(&conn_.rev) || should_break) {
      return 0;
    }

    auto nread = conn_.read_tls(rb_.last(), rb_.wleft());

    if (nread == 0) {
      if (rb_.rleft() == 0) {
        rb_.release_chunk();
      }
      return 0;
    }

    if (nread < 0) {
      return -1;
    }

    rb_.write(nread);
    should_break = true;
  }
}

int ClientHandler::write_tls() {
  struct iovec iov;

  ERR_clear_error();

  if (on_write() != 0) {
    return -1;
  }

  auto iovcnt = upstream_->response_riovec(&iov, 1);
  if (iovcnt == 0) {
    conn_.start_tls_write_idle();

    conn_.wlimit.stopw();
    ev_timer_stop(conn_.loop, &conn_.wt);

    return 0;
  }

  for (;;) {
    auto nwrite = conn_.write_tls(iov.iov_base, iov.iov_len);
    if (nwrite < 0) {
      return -1;
    }

    if (nwrite == 0) {
      return 0;
    }

    upstream_->response_drain(nwrite);

    iovcnt = upstream_->response_riovec(&iov, 1);
    if (iovcnt == 0) {
      return 0;
    }
  }
}

#ifdef ENABLE_HTTP3
int ClientHandler::read_quic(const UpstreamAddr *faddr,
                             const Address &remote_addr,
                             const Address &local_addr,
                             const ngtcp2_pkt_info &pi,
                             std::span<const uint8_t> data) {
  auto upstream = static_cast<Http3Upstream *>(upstream_.get());

  return upstream->on_read(faddr, remote_addr, local_addr, pi, data);
}

int ClientHandler::write_quic() { return upstream_->on_write(); }
#endif // ENABLE_HTTP3

int ClientHandler::upstream_noop() { return 0; }

int ClientHandler::upstream_read() {
  assert(upstream_);
  if (upstream_->on_read() != 0) {
    return -1;
  }
  return 0;
}

int ClientHandler::upstream_write() {
  assert(upstream_);
  if (upstream_->on_write() != 0) {
    return -1;
  }

  if (get_should_close_after_write() && upstream_->response_empty()) {
    return -1;
  }

  return 0;
}

int ClientHandler::upstream_http2_connhd_read() {
  auto nread = std::min(left_connhd_len_, rb_.rleft());
  if (memcmp(&NGHTTP2_CLIENT_MAGIC[NGHTTP2_CLIENT_MAGIC_LEN - left_connhd_len_],
             rb_.pos(), nread) != 0) {
    // There is no downgrade path here. Just drop the connection.
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "invalid client connection header";
    }

    return -1;
  }

  left_connhd_len_ -= nread;
  rb_.drain(nread);
  conn_.rlimit.startw();

  if (left_connhd_len_ == 0) {
    on_read_ = &ClientHandler::upstream_read;
    // Run on_read to process data left in buffer since they are not
    // notified further
    if (on_read() != 0) {
      return -1;
    }
    return 0;
  }

  return 0;
}

int ClientHandler::upstream_http1_connhd_read() {
  auto nread = std::min(left_connhd_len_, rb_.rleft());
  if (memcmp(&NGHTTP2_CLIENT_MAGIC[NGHTTP2_CLIENT_MAGIC_LEN - left_connhd_len_],
             rb_.pos(), nread) != 0) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "This is HTTP/1.1 connection, "
                       << "but may be upgraded to HTTP/2 later.";
    }

    // Reset header length for later HTTP/2 upgrade
    left_connhd_len_ = NGHTTP2_CLIENT_MAGIC_LEN;
    on_read_ = &ClientHandler::upstream_read;
    on_write_ = &ClientHandler::upstream_write;

    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  left_connhd_len_ -= nread;
  rb_.drain(nread);
  conn_.rlimit.startw();

  if (left_connhd_len_ == 0) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "direct HTTP/2 connection";
    }

    direct_http2_upgrade();
    on_read_ = &ClientHandler::upstream_read;
    on_write_ = &ClientHandler::upstream_write;

    // Run on_read to process data left in buffer since they are not
    // notified further
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  return 0;
}

ClientHandler::ClientHandler(Worker *worker, int fd, SSL *ssl,
                             const StringRef &ipaddr, const StringRef &port,
                             int family, const UpstreamAddr *faddr)
  : // We use balloc_ for TLS session ID (64), ipaddr (IPv6) (39),
    // port (5), forwarded-for (IPv6) (41), alpn (5), proxyproto
    // ipaddr (15), proxyproto port (5), sni (32, estimated).  we
    // need terminal NULL byte for each.  We also require 8 bytes
    // header for each allocation.  We align at 16 bytes boundary,
    // so the required space is 64 + 48 + 16 + 48 + 16 + 16 + 16 +
    // 32 + 8 + 8 * 8 = 328.
    balloc_(512, 512),
    rb_(worker->get_mcpool()),
    conn_(worker->get_loop(), fd, ssl, worker->get_mcpool(),
          get_config()->conn.upstream.timeout.write,
          get_config()->conn.upstream.timeout.idle,
          get_config()->conn.upstream.ratelimit.write,
          get_config()->conn.upstream.ratelimit.read, writecb, readcb,
          timeoutcb, this, get_config()->tls.dyn_rec.warmup_threshold,
          get_config()->tls.dyn_rec.idle_timeout,
          faddr->quic ? Proto::HTTP3 : Proto::NONE),
    ipaddr_(make_string_ref(balloc_, ipaddr)),
    port_(make_string_ref(balloc_, port)),
    faddr_(faddr),
    worker_(worker),
    left_connhd_len_(NGHTTP2_CLIENT_MAGIC_LEN),
    affinity_hash_(0),
    should_close_after_write_(false),
    affinity_hash_computed_(false) {
  ++worker_->get_worker_stat()->num_connections;

  ev_timer_init(&reneg_shutdown_timer_, shutdowncb, 0., 0.);

  reneg_shutdown_timer_.data = this;

  if (!faddr->quic) {
    conn_.rlimit.startw();
  }
  ev_timer_again(conn_.loop, &conn_.rt);

  auto config = get_config();

  if (!faddr->quic) {
    if (faddr_->accept_proxy_protocol ||
        config->conn.upstream.accept_proxy_protocol) {
      read_ = &ClientHandler::proxy_protocol_peek_clear;
      write_ = &ClientHandler::noop;
      on_read_ = &ClientHandler::proxy_protocol_read;
      on_write_ = &ClientHandler::upstream_noop;
    } else {
      setup_upstream_io_callback();
    }
  }

  auto &fwdconf = config->http.forwarded;

  if (fwdconf.params & FORWARDED_FOR) {
    if (fwdconf.for_node_type == ForwardedNode::OBFUSCATED) {
      // 1 for '_'
      auto len = SHRPX_OBFUSCATED_NODE_LENGTH + 1;
      // 1 for terminating NUL.
      auto buf = make_byte_ref(balloc_, len + 1);
      auto p = std::begin(buf);
      *p++ = '_';
      p = util::random_alpha_digit(p, p + SHRPX_OBFUSCATED_NODE_LENGTH,
                                   worker_->get_randgen());
      *p = '\0';

      forwarded_for_ = StringRef{std::span{std::begin(buf), p}};
    } else {
      init_forwarded_for(family, ipaddr_);
    }
  }
}

void ClientHandler::init_forwarded_for(int family, const StringRef &ipaddr) {
  if (family == AF_INET6) {
    // 2 for '[' and ']'
    auto len = 2 + ipaddr.size();
    // 1 for terminating NUL.
    auto buf = make_byte_ref(balloc_, len + 1);
    auto p = std::begin(buf);
    *p++ = '[';
    p = std::copy(std::begin(ipaddr), std::end(ipaddr), p);
    *p++ = ']';
    *p = '\0';

    forwarded_for_ = StringRef{std::span{std::begin(buf), p}};
  } else {
    // family == AF_INET or family == AF_UNIX
    forwarded_for_ = ipaddr;
  }
}

void ClientHandler::setup_upstream_io_callback() {
  if (conn_.tls.ssl) {
    conn_.prepare_server_handshake();
    read_ = write_ = &ClientHandler::tls_handshake;
    on_read_ = &ClientHandler::upstream_noop;
    on_write_ = &ClientHandler::upstream_write;
  } else {
    // For non-TLS version, first create HttpsUpstream. It may be
    // upgraded to HTTP/2 through HTTP Upgrade or direct HTTP/2
    // connection.
    upstream_ = std::make_unique<HttpsUpstream>(this);
    alpn_ = "http/1.1"_sr;
    read_ = &ClientHandler::read_clear;
    write_ = &ClientHandler::write_clear;
    on_read_ = &ClientHandler::upstream_http1_connhd_read;
    on_write_ = &ClientHandler::upstream_noop;
  }
}

#ifdef ENABLE_HTTP3
void ClientHandler::setup_http3_upstream(
  std::unique_ptr<Http3Upstream> &&upstream) {
  upstream_ = std::move(upstream);
  write_ = &ClientHandler::write_quic;

  auto config = get_config();

  reset_upstream_read_timeout(config->conn.upstream.timeout.http3_idle);
}
#endif // ENABLE_HTTP3

ClientHandler::~ClientHandler() {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Deleting";
  }

  if (upstream_) {
    upstream_->on_handler_delete();
  }

  auto worker_stat = worker_->get_worker_stat();
  --worker_stat->num_connections;

  if (worker_stat->num_connections == 0) {
    worker_->schedule_clear_mcpool();
  }

  ev_timer_stop(conn_.loop, &reneg_shutdown_timer_);

  // TODO If backend is http/2, and it is in CONNECTED state, signal
  // it and make it loopbreak when output is zero.
  if (worker_->get_graceful_shutdown() && worker_stat->num_connections == 0 &&
      worker_stat->num_close_waits == 0) {
    ev_break(conn_.loop);
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Deleted";
  }
}

Upstream *ClientHandler::get_upstream() { return upstream_.get(); }

struct ev_loop *ClientHandler::get_loop() const { return conn_.loop; }

void ClientHandler::reset_upstream_read_timeout(ev_tstamp t) {
  conn_.rt.repeat = t;

  ev_timer_again(conn_.loop, &conn_.rt);
}

void ClientHandler::reset_upstream_write_timeout(ev_tstamp t) {
  conn_.wt.repeat = t;

  ev_timer_again(conn_.loop, &conn_.wt);
}

void ClientHandler::repeat_read_timer() {
  ev_timer_again(conn_.loop, &conn_.rt);
}

void ClientHandler::stop_read_timer() { ev_timer_stop(conn_.loop, &conn_.rt); }

int ClientHandler::validate_next_proto() {
  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len = 0;

  // First set callback for catch all cases
  on_read_ = &ClientHandler::upstream_read;

  SSL_get0_alpn_selected(conn_.tls.ssl, &next_proto, &next_proto_len);

  StringRef proto;

  if (next_proto) {
    proto = StringRef{next_proto, next_proto_len};

    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "The negotiated next protocol: " << proto;
    }
  } else {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "No protocol negotiated. Fallback to HTTP/1.1";
    }

    proto = "http/1.1"_sr;
  }

  if (!tls::in_proto_list(get_config()->tls.alpn_list, proto)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "The negotiated protocol is not supported: " << proto;
    }
    return -1;
  }

  if (util::check_h2_is_selected(proto)) {
    on_read_ = &ClientHandler::upstream_http2_connhd_read;

    auto http2_upstream = std::make_unique<Http2Upstream>(this);

    upstream_ = std::move(http2_upstream);
    alpn_ = make_string_ref(balloc_, proto);

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }

  if (proto == "http/1.1"_sr) {
    upstream_ = std::make_unique<HttpsUpstream>(this);
    alpn_ = "http/1.1"_sr;

    // At this point, input buffer is already filled with some bytes.
    // The read callback is not called until new data come. So consume
    // input buffer here.
    if (on_read() != 0) {
      return -1;
    }

    return 0;
  }
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "The negotiated protocol is not supported";
  }
  return -1;
}

int ClientHandler::do_read() { return read_(*this); }
int ClientHandler::do_write() { return write_(*this); }

int ClientHandler::on_read() {
  if (rb_.chunk_avail()) {
    auto rv = on_read_(*this);
    if (rv != 0) {
      return rv;
    }
  }
  conn_.handle_tls_pending_read();
  return 0;
}
int ClientHandler::on_write() { return on_write_(*this); }

const StringRef &ClientHandler::get_ipaddr() const { return ipaddr_; }

bool ClientHandler::get_should_close_after_write() const {
  return should_close_after_write_;
}

void ClientHandler::set_should_close_after_write(bool f) {
  should_close_after_write_ = f;
}

void ClientHandler::pool_downstream_connection(
  std::unique_ptr<DownstreamConnection> dconn) {
  if (!dconn->poolable()) {
    return;
  }

  dconn->set_client_handler(nullptr);

  auto &group = dconn->get_downstream_addr_group();

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Pooling downstream connection DCONN:" << dconn.get()
                     << " in group " << group;
  }

  auto addr = dconn->get_addr();
  auto &dconn_pool = addr->dconn_pool;
  dconn_pool->add_downstream_connection(std::move(dconn));
}

namespace {
// Computes 32bits hash for session affinity for IP address |ip|.
uint32_t compute_affinity_from_ip(const StringRef &ip) {
  int rv;
  std::array<uint8_t, 32> buf;

  rv = util::sha256(buf.data(), ip);
  if (rv != 0) {
    // Not sure when sha256 failed.  Just fall back to another
    // function.
    return util::hash32(ip);
  }

  return (static_cast<uint32_t>(buf[0]) << 24) |
         (static_cast<uint32_t>(buf[1]) << 16) |
         (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}
} // namespace

Http2Session *ClientHandler::get_http2_session(
  const std::shared_ptr<DownstreamAddrGroup> &group, DownstreamAddr *addr) {
  auto &shared_addr = group->shared_addr;

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Selected DownstreamAddr=" << addr
                     << ", index=" << (addr - shared_addr->addrs.data());
  }

  for (auto session = addr->http2_extra_freelist.head; session;) {
    auto next = session->dlnext;

    if (session->max_concurrency_reached(0)) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this)
          << "Maximum streams have been reached for Http2Session(" << session
          << ").  Skip it";
      }

      session->remove_from_freelist();
      session = next;

      continue;
    }

    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "Use Http2Session " << session
                       << " from http2_extra_freelist";
    }

    if (session->max_concurrency_reached(1)) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "Maximum streams are reached for Http2Session("
                         << session << ").";
      }

      session->remove_from_freelist();
    }
    return session;
  }

  auto session = new Http2Session(conn_.loop, worker_->get_cl_ssl_ctx(),
                                  worker_, group, addr);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Create new Http2Session " << session;
  }

  session->add_to_extra_freelist();

  return session;
}

uint32_t ClientHandler::get_affinity_cookie(Downstream *downstream,
                                            const StringRef &cookie_name) {
  auto h = downstream->find_affinity_cookie(cookie_name);
  if (h) {
    return h;
  }

  auto d = std::uniform_int_distribution<uint32_t>(1);
  auto rh = d(worker_->get_randgen());
  h = util::hash32(StringRef{reinterpret_cast<char *>(&rh), sizeof(rh)});

  downstream->renew_affinity_cookie(h);

  return h;
}

namespace {
void reschedule_addr(
  std::priority_queue<DownstreamAddrEntry, std::vector<DownstreamAddrEntry>,
                      DownstreamAddrEntryGreater> &pq,
  DownstreamAddr *addr) {
  auto penalty = MAX_DOWNSTREAM_ADDR_WEIGHT + addr->pending_penalty;
  addr->cycle += penalty / addr->weight;
  addr->pending_penalty = penalty % addr->weight;

  pq.push(DownstreamAddrEntry{addr, addr->seq, addr->cycle});
  addr->queued = true;
}
} // namespace

namespace {
void reschedule_wg(
  std::priority_queue<WeightGroupEntry, std::vector<WeightGroupEntry>,
                      WeightGroupEntryGreater> &pq,
  WeightGroup *wg) {
  auto penalty = MAX_DOWNSTREAM_ADDR_WEIGHT + wg->pending_penalty;
  wg->cycle += penalty / wg->weight;
  wg->pending_penalty = penalty % wg->weight;

  pq.push(WeightGroupEntry{wg, wg->seq, wg->cycle});
  wg->queued = true;
}
} // namespace

DownstreamAddr *ClientHandler::get_downstream_addr(int &err,
                                                   DownstreamAddrGroup *group,
                                                   Downstream *downstream) {
  err = 0;

  switch (faddr_->alt_mode) {
  case UpstreamAltMode::API:
  case UpstreamAltMode::HEALTHMON:
    assert(0);
  default:
    break;
  }

  auto &shared_addr = group->shared_addr;

  if (shared_addr->affinity.type != SessionAffinity::NONE) {
    uint32_t hash;
    switch (shared_addr->affinity.type) {
    case SessionAffinity::IP:
      if (!affinity_hash_computed_) {
        affinity_hash_ = compute_affinity_from_ip(ipaddr_);
        affinity_hash_computed_ = true;
      }
      hash = affinity_hash_;
      break;
    case SessionAffinity::COOKIE:
      if (shared_addr->affinity.cookie.stickiness ==
          SessionAffinityCookieStickiness::STRICT) {
        return get_downstream_addr_strict_affinity(err, shared_addr,
                                                   downstream);
      }

      hash = get_affinity_cookie(downstream, shared_addr->affinity.cookie.name);
      break;
    default:
      assert(0);
    }

    const auto &affinity_hash = shared_addr->affinity_hash;

    auto it = std::lower_bound(
      std::begin(affinity_hash), std::end(affinity_hash), hash,
      [](const AffinityHash &lhs, uint32_t rhs) { return lhs.hash < rhs; });

    if (it == std::end(affinity_hash)) {
      it = std::begin(affinity_hash);
    }

    auto aff_idx =
      static_cast<size_t>(std::distance(std::begin(affinity_hash), it));
    auto idx = (*it).idx;
    auto addr = &shared_addr->addrs[idx];

    if (addr->connect_blocker->blocked()) {
      size_t i;
      for (i = aff_idx + 1; i != aff_idx; ++i) {
        if (i == shared_addr->affinity_hash.size()) {
          i = 0;
        }
        addr = &shared_addr->addrs[shared_addr->affinity_hash[i].idx];
        if (addr->connect_blocker->blocked()) {
          continue;
        }
        break;
      }
      if (i == aff_idx) {
        err = -1;
        return nullptr;
      }
    }

    return addr;
  }

  auto &wgpq = shared_addr->pq;

  for (;;) {
    if (wgpq.empty()) {
      CLOG(INFO, this) << "No working downstream address found";
      err = -1;
      return nullptr;
    }

    auto wg = wgpq.top().wg;
    wgpq.pop();
    wg->queued = false;

    for (;;) {
      if (wg->pq.empty()) {
        break;
      }

      auto addr = wg->pq.top().addr;
      wg->pq.pop();
      addr->queued = false;

      if (addr->connect_blocker->blocked()) {
        continue;
      }

      reschedule_addr(wg->pq, addr);
      reschedule_wg(wgpq, wg);

      return addr;
    }
  }
}

DownstreamAddr *ClientHandler::get_downstream_addr_strict_affinity(
  int &err, const std::shared_ptr<SharedDownstreamAddr> &shared_addr,
  Downstream *downstream) {
  const auto &affinity_hash = shared_addr->affinity_hash;

  auto h = downstream->find_affinity_cookie(shared_addr->affinity.cookie.name);
  if (h) {
    auto it = shared_addr->affinity_hash_map.find(h);
    if (it != std::end(shared_addr->affinity_hash_map)) {
      auto addr = &shared_addr->addrs[(*it).second];
      if (!addr->connect_blocker->blocked()) {
        return addr;
      }
    }
  } else {
    auto d = std::uniform_int_distribution<uint32_t>(1);
    auto rh = d(worker_->get_randgen());
    h = util::hash32(StringRef{reinterpret_cast<char *>(&rh), sizeof(rh)});
  }

  // Client is not bound to a particular backend, or the bound backend
  // is not found, or is blocked.  Find new backend using h.  Using
  // existing h allows us to find new server in a deterministic way.
  // It is preferable because multiple concurrent requests with the
  // stale cookie might be in-flight.
  auto it = std::lower_bound(
    std::begin(affinity_hash), std::end(affinity_hash), h,
    [](const AffinityHash &lhs, uint32_t rhs) { return lhs.hash < rhs; });

  if (it == std::end(affinity_hash)) {
    it = std::begin(affinity_hash);
  }

  auto aff_idx =
    static_cast<size_t>(std::distance(std::begin(affinity_hash), it));
  auto idx = (*it).idx;
  auto addr = &shared_addr->addrs[idx];

  if (addr->connect_blocker->blocked()) {
    size_t i;
    for (i = aff_idx + 1; i != aff_idx; ++i) {
      if (i == shared_addr->affinity_hash.size()) {
        i = 0;
      }
      addr = &shared_addr->addrs[shared_addr->affinity_hash[i].idx];
      if (addr->connect_blocker->blocked()) {
        continue;
      }
      break;
    }
    if (i == aff_idx) {
      err = -1;
      return nullptr;
    }
  }

  downstream->renew_affinity_cookie(addr->affinity_hash);

  return addr;
}

std::unique_ptr<DownstreamConnection>
ClientHandler::get_downstream_connection(int &err, Downstream *downstream) {
  size_t group_idx;
  auto &downstreamconf = *worker_->get_downstream_config();
  auto &routerconf = downstreamconf.router;

  auto catch_all = downstreamconf.addr_group_catch_all;
  auto &groups = worker_->get_downstream_addr_groups();

  auto &req = downstream->request();

  err = 0;

  switch (faddr_->alt_mode) {
  case UpstreamAltMode::API: {
    auto dconn = std::make_unique<APIDownstreamConnection>(worker_);
    dconn->set_client_handler(this);
    return dconn;
  }
  case UpstreamAltMode::HEALTHMON: {
    auto dconn = std::make_unique<HealthMonitorDownstreamConnection>();
    dconn->set_client_handler(this);
    return dconn;
  }
  default:
    break;
  }

  auto &balloc = downstream->get_block_allocator();

  StringRef authority, path;

  if (req.forwarded_once) {
    if (groups.size() != 1) {
      authority = req.orig_authority;
      path = req.orig_path;
    }
  } else {
    if (faddr_->sni_fwd) {
      authority = sni_;
    } else if (!req.authority.empty()) {
      authority = req.authority;
    } else {
      auto h = req.fs.header(http2::HD_HOST);
      if (h) {
        authority = h->value;
      }
    }

    // CONNECT method does not have path.  But we requires path in
    // host-path mapping.  As workaround, we assume that path is
    // "/".
    if (!req.regular_connect_method()) {
      path = req.path;
    }

    // Cache the authority and path used for the first-time backend
    // selection because per-pattern mruby script can change them.
    req.orig_authority = authority;
    req.orig_path = path;
    req.forwarded_once = true;
  }

  // Fast path.  If we have one group, it must be catch-all group.
  if (groups.size() == 1) {
    group_idx = 0;
  } else {
    group_idx = match_downstream_addr_group(routerconf, authority, path, groups,
                                            catch_all, balloc);
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Downstream address group_idx: " << group_idx;
  }

  if (groups[group_idx]->shared_addr->redirect_if_not_tls && !conn_.tls.ssl) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "Downstream address group " << group_idx
                       << " requires frontend TLS connection.";
    }
    err = SHRPX_ERR_TLS_REQUIRED;
    return nullptr;
  }

  auto &group = groups[group_idx];

  if (group->shared_addr->dnf) {
    auto dconn = std::make_unique<NullDownstreamConnection>(group);
    dconn->set_client_handler(this);
    return dconn;
  }

  auto addr = get_downstream_addr(err, group.get(), downstream);
  if (addr == nullptr) {
    return nullptr;
  }

  if (addr->proto == Proto::HTTP1) {
    auto dconn = addr->dconn_pool->pop_downstream_connection();
    if (dconn) {
      dconn->set_client_handler(this);
      return dconn;
    }

    if (worker_->get_connect_blocker()->blocked()) {
      if (LOG_ENABLED(INFO)) {
        DCLOG(INFO, this)
          << "Worker wide backend connection was blocked temporarily";
      }
      return nullptr;
    }

    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "Downstream connection pool is empty."
                       << " Create new one";
    }

    dconn = std::make_unique<HttpDownstreamConnection>(group, addr, conn_.loop,
                                                       worker_);
    dconn->set_client_handler(this);
    return dconn;
  }

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "Downstream connection pool is empty."
                     << " Create new one";
  }

  auto http2session = get_http2_session(group, addr);
  auto dconn = std::make_unique<Http2DownstreamConnection>(http2session);
  dconn->set_client_handler(this);
  return dconn;
}

MemchunkPool *ClientHandler::get_mcpool() { return worker_->get_mcpool(); }

SSL *ClientHandler::get_ssl() const { return conn_.tls.ssl; }

void ClientHandler::direct_http2_upgrade() {
  upstream_ = std::make_unique<Http2Upstream>(this);
  alpn_ = NGHTTP2_CLEARTEXT_PROTO_VERSION_ID ""_sr;
  on_read_ = &ClientHandler::upstream_read;
  write_ = &ClientHandler::write_clear;
}

int ClientHandler::perform_http2_upgrade(HttpsUpstream *http) {
  auto upstream = std::make_unique<Http2Upstream>(this);

  auto output = upstream->get_response_buf();

  // We might have written non-final header in response_buf, in this
  // case, response_state is still INITIAL.  If this non-final header
  // and upgrade header fit in output buffer, do upgrade.  Otherwise,
  // to avoid to send this non-final header as response body in HTTP/2
  // upstream, fail upgrade.
  auto downstream = http->get_downstream();
  auto input = downstream->get_response_buf();

  if (upstream->upgrade_upstream(http) != 0) {
    return -1;
  }
  // http pointer is now owned by upstream.
  upstream_.release();
  // TODO We might get other version id in HTTP2-settings, if we
  // support aliasing for h2, but we just use library default for now.
  alpn_ = NGHTTP2_CLEARTEXT_PROTO_VERSION_ID ""_sr;
  on_read_ = &ClientHandler::upstream_http2_connhd_read;
  write_ = &ClientHandler::write_clear;

  input->remove(*output, input->rleft());

  constexpr auto res = "HTTP/1.1 101 Switching Protocols\r\n"
                       "Connection: Upgrade\r\n"
                       "Upgrade: " NGHTTP2_CLEARTEXT_PROTO_VERSION_ID "\r\n"
                       "\r\n"_sr;

  output->append(res);
  upstream_ = std::move(upstream);

  signal_write();
  return 0;
}

bool ClientHandler::get_http2_upgrade_allowed() const { return !conn_.tls.ssl; }

StringRef ClientHandler::get_upstream_scheme() const {
  if (conn_.tls.ssl) {
    return "https"_sr;
  } else {
    return "http"_sr;
  }
}

void ClientHandler::start_immediate_shutdown() {
  ev_timer_start(conn_.loop, &reneg_shutdown_timer_);
}

void ClientHandler::write_accesslog(Downstream *downstream) {
  auto &req = downstream->request();

  auto config = get_config();

  if (!req.tstamp) {
    auto lgconf = log_config();
    lgconf->update_tstamp(std::chrono::system_clock::now());
    req.tstamp = lgconf->tstamp;
  }

  upstream_accesslog(
    config->logging.access.format,
    LogSpec{
      downstream,
      ipaddr_,
      alpn_,
      sni_,
      conn_.tls.ssl,
      std::chrono::high_resolution_clock::now(), // request_end_time
      port_,
      faddr_->port,
      config->pid,
    });
}

ClientHandler::ReadBuf *ClientHandler::get_rb() { return &rb_; }

void ClientHandler::signal_write() { conn_.wlimit.startw(); }

RateLimit *ClientHandler::get_rlimit() { return &conn_.rlimit; }
RateLimit *ClientHandler::get_wlimit() { return &conn_.wlimit; }

ev_io *ClientHandler::get_wev() { return &conn_.wev; }

Worker *ClientHandler::get_worker() const { return worker_; }

namespace {
ssize_t parse_proxy_line_port(const uint8_t *first, const uint8_t *last) {
  auto p = first;
  int32_t port = 0;

  if (p == last) {
    return -1;
  }

  if (*p == '0') {
    if (p + 1 != last && util::is_digit(*(p + 1))) {
      return -1;
    }
    return 1;
  }

  for (; p != last && util::is_digit(*p); ++p) {
    port *= 10;
    port += *p - '0';

    if (port > 65535) {
      return -1;
    }
  }

  return p - first;
}
} // namespace

int ClientHandler::on_proxy_protocol_finish() {
  auto len = rb_.pos() - rb_.begin();

  assert(len);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol: Draining " << len
                     << " bytes from socket";
  }

  rb_.reset();

  if (conn_.read_nolim_clear(rb_.pos(), len) < 0) {
    return -1;
  }

  rb_.reset();

  setup_upstream_io_callback();

  return 0;
}

namespace {
// PROXY-protocol v2 header signature
constexpr uint8_t PROXY_PROTO_V2_SIG[] =
  "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A";

// PROXY-protocol v2 header length
constexpr size_t PROXY_PROTO_V2_HDLEN =
  str_size(PROXY_PROTO_V2_SIG) + /* ver_cmd(1) + fam(1) + len(2) = */ 4;
} // namespace

// http://www.haproxy.org/download/1.5/doc/proxy-protocol.txt
int ClientHandler::proxy_protocol_read() {
  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol: Started";
  }

  auto first = rb_.pos();

  if (rb_.rleft() >= PROXY_PROTO_V2_HDLEN &&
      (*(first + str_size(PROXY_PROTO_V2_SIG)) & 0xf0) == 0x20) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol: Detected v2 header signature";
    }
    return proxy_protocol_v2_read();
  }

  // NULL character really destroys functions which expects NULL
  // terminated string.  We won't expect it in PROXY protocol line, so
  // find it here.
  auto chrs = std::to_array({'\n', '\0'});

  constexpr size_t MAX_PROXY_LINELEN = 107;

  auto bufend = rb_.pos() + std::min(MAX_PROXY_LINELEN, rb_.rleft());

  auto end =
    std::find_first_of(rb_.pos(), bufend, std::begin(chrs), std::end(chrs));

  if (end == bufend || *end == '\0' || end == rb_.pos() || *(end - 1) != '\r') {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: No ending CR LF sequence found";
    }
    return -1;
  }

  --end;

  constexpr auto HEADER = "PROXY "_sr;

  if (static_cast<size_t>(end - rb_.pos()) < HEADER.size()) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: PROXY version 1 ID not found";
    }
    return -1;
  }

  if (HEADER != StringRef{rb_.pos(), HEADER.size()}) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Bad PROXY protocol version 1 ID";
    }
    return -1;
  }

  rb_.drain(HEADER.size());

  int family;

  if (rb_.pos()[0] == 'T') {
    if (end - rb_.pos() < 5) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: INET protocol family not found";
      }
      return -1;
    }

    if (rb_.pos()[1] != 'C' || rb_.pos()[2] != 'P') {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    switch (rb_.pos()[3]) {
    case '4':
      family = AF_INET;
      break;
    case '6':
      family = AF_INET6;
      break;
    default:
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    rb_.drain(5);
  } else {
    if (end - rb_.pos() < 7) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: INET protocol family not found";
      }
      return -1;
    }
    if ("UNKNOWN"_sr != StringRef{rb_.pos(), 7}) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v1: Unknown INET protocol family";
      }
      return -1;
    }

    rb_.drain(end + 2 - rb_.pos());

    return on_proxy_protocol_finish();
  }

  // source address
  auto token_end = std::find(rb_.pos(), end, ' ');
  if (token_end == end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Source address not found";
    }
    return -1;
  }

  *token_end = '\0';
  if (!util::numeric_host(reinterpret_cast<const char *>(rb_.pos()), family)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid source address";
    }
    return -1;
  }

  auto src_addr = rb_.pos();
  auto src_addrlen = token_end - rb_.pos();

  rb_.drain(token_end - rb_.pos() + 1);

  // destination address
  token_end = std::find(rb_.pos(), end, ' ');
  if (token_end == end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Destination address not found";
    }
    return -1;
  }

  *token_end = '\0';
  if (!util::numeric_host(reinterpret_cast<const char *>(rb_.pos()), family)) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid destination address";
    }
    return -1;
  }

  // Currently we don't use destination address

  rb_.drain(token_end - rb_.pos() + 1);

  // source port
  auto n = parse_proxy_line_port(rb_.pos(), end);
  if (n <= 0 || *(rb_.pos() + n) != ' ') {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid source port";
    }
    return -1;
  }

  rb_.pos()[n] = '\0';
  auto src_port = rb_.pos();
  auto src_portlen = n;

  rb_.drain(n + 1);

  // destination  port
  n = parse_proxy_line_port(rb_.pos(), end);
  if (n <= 0 || rb_.pos() + n != end) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v1: Invalid destination port";
    }
    return -1;
  }

  // Currently we don't use destination port

  rb_.drain(end + 2 - rb_.pos());

  ipaddr_ = make_string_ref(
    balloc_, StringRef{src_addr, static_cast<size_t>(src_addrlen)});
  port_ = make_string_ref(
    balloc_, StringRef{src_port, static_cast<size_t>(src_portlen)});

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol-v1: Finished, " << (rb_.pos() - first)
                     << " bytes read";
  }

  auto config = get_config();
  auto &fwdconf = config->http.forwarded;

  if ((fwdconf.params & FORWARDED_FOR) &&
      fwdconf.for_node_type == ForwardedNode::IP) {
    init_forwarded_for(family, ipaddr_);
  }

  return on_proxy_protocol_finish();
}

int ClientHandler::proxy_protocol_v2_read() {
  // Assume that first str_size(PROXY_PROTO_V2_SIG) octets match v2
  // protocol signature and followed by the bytes which indicates v2.
  assert(rb_.rleft() >= PROXY_PROTO_V2_HDLEN);

  auto p = rb_.pos() + str_size(PROXY_PROTO_V2_SIG);

  assert(((*p) & 0xf0) == 0x20);

  enum { LOCAL, PROXY } cmd;

  auto cmd_bits = (*p++) & 0xf;
  switch (cmd_bits) {
  case 0x0:
    cmd = LOCAL;
    break;
  case 0x01:
    cmd = PROXY;
    break;
  default:
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v2: Unknown command " << log::hex
                       << cmd_bits;
    }
    return -1;
  }

  auto fam = *p++;
  uint16_t len;
  memcpy(&len, p, sizeof(len));
  len = ntohs(len);

  p += sizeof(len);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol-v2: Detected family=" << log::hex << fam
                     << ", len=" << log::dec << len;
  }

  if (rb_.last() - p < len) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this)
        << "PROXY-protocol-v2: Prematurely truncated header block; require "
        << len << " bytes, " << rb_.last() - p << " bytes left";
    }
    return -1;
  }

  int family;
  std::array<char, std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)> src_addr,
    dst_addr;
  size_t addrlen;

  switch (fam) {
  case 0x11:
  case 0x12:
    if (len < 12) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v2: Too short AF_INET addresses";
      }
      return -1;
    }
    family = AF_INET;
    addrlen = 4;
    break;
  case 0x21:
  case 0x22:
    if (len < 36) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v2: Too short AF_INET6 addresses";
      }
      return -1;
    }
    family = AF_INET6;
    addrlen = 16;
    break;
  case 0x31:
  case 0x32:
    if (len < 216) {
      if (LOG_ENABLED(INFO)) {
        CLOG(INFO, this) << "PROXY-protocol-v2: Too short AF_UNIX addresses";
      }
      return -1;
    }
    // fall through
  case 0x00: {
    // UNSPEC and UNIX are just ignored.
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v2: Ignore combination of address "
                          "family and protocol "
                       << log::hex << fam;
    }
    rb_.drain(PROXY_PROTO_V2_HDLEN + len);
    return on_proxy_protocol_finish();
  }
  default:
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v2: Unknown combination of address "
                          "family and protocol "
                       << log::hex << fam;
    }
    return -1;
  }

  if (cmd != PROXY) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v2: Ignore non-PROXY command";
    }
    rb_.drain(PROXY_PROTO_V2_HDLEN + len);
    return on_proxy_protocol_finish();
  }

  if (inet_ntop(family, p, src_addr.data(), src_addr.size()) == nullptr) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this) << "PROXY-protocol-v2: Unable to parse source address";
    }
    return -1;
  }

  p += addrlen;

  if (inet_ntop(family, p, dst_addr.data(), dst_addr.size()) == nullptr) {
    if (LOG_ENABLED(INFO)) {
      CLOG(INFO, this)
        << "PROXY-protocol-v2: Unable to parse destination address";
    }
    return -1;
  }

  p += addrlen;

  uint16_t src_port;

  memcpy(&src_port, p, sizeof(src_port));
  src_port = ntohs(src_port);

  // We don't use destination port.
  p += 4;

  ipaddr_ = make_string_ref(balloc_, StringRef{src_addr.data()});
  port_ = util::make_string_ref_uint(balloc_, src_port);

  if (LOG_ENABLED(INFO)) {
    CLOG(INFO, this) << "PROXY-protocol-v2: Finished reading proxy addresses, "
                     << p - rb_.pos() << " bytes read, "
                     << PROXY_PROTO_V2_HDLEN + len - (p - rb_.pos())
                     << " bytes left";
  }

  auto config = get_config();
  auto &fwdconf = config->http.forwarded;

  if ((fwdconf.params & FORWARDED_FOR) &&
      fwdconf.for_node_type == ForwardedNode::IP) {
    init_forwarded_for(family, ipaddr_);
  }

  rb_.drain(PROXY_PROTO_V2_HDLEN + len);
  return on_proxy_protocol_finish();
}

StringRef ClientHandler::get_forwarded_by() const {
  auto &fwdconf = get_config()->http.forwarded;

  if (fwdconf.by_node_type == ForwardedNode::OBFUSCATED) {
    return fwdconf.by_obfuscated;
  }

  return faddr_->hostport;
}

StringRef ClientHandler::get_forwarded_for() const { return forwarded_for_; }

const UpstreamAddr *ClientHandler::get_upstream_addr() const { return faddr_; }

Connection *ClientHandler::get_connection() { return &conn_; }

void ClientHandler::set_tls_sni(const StringRef &sni) {
  sni_ = make_string_ref(balloc_, sni);
}

StringRef ClientHandler::get_tls_sni() const { return sni_; }

StringRef ClientHandler::get_alpn() const { return alpn_; }

BlockAllocator &ClientHandler::get_block_allocator() { return balloc_; }

void ClientHandler::set_alpn_from_conn() {
  const unsigned char *alpn;
  unsigned int alpnlen;

  SSL_get0_alpn_selected(conn_.tls.ssl, &alpn, &alpnlen);

  alpn_ = make_string_ref(balloc_, StringRef{alpn, alpnlen});
}

} // namespace shrpx
