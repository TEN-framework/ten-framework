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
#ifndef SHRPX_CLIENT_HANDLER_H
#define SHRPX_CLIENT_HANDLER_H

#include "shrpx.h"

#include <memory>

#include <ev.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include "shrpx_rate_limit.h"
#include "shrpx_connection.h"
#include "buffer.h"
#include "memchunk.h"
#include "allocator.h"

using namespace nghttp2;

namespace shrpx {

class Upstream;
class DownstreamConnection;
class HttpsUpstream;
class ConnectBlocker;
class DownstreamConnectionPool;
class Worker;
class Downstream;
struct WorkerStat;
struct DownstreamAddrGroup;
struct SharedDownstreamAddr;
struct DownstreamAddr;
#ifdef ENABLE_HTTP3
class Http3Upstream;
#endif // ENABLE_HTTP3

class ClientHandler {
public:
  ClientHandler(Worker *worker, int fd, SSL *ssl, const StringRef &ipaddr,
                const StringRef &port, int family, const UpstreamAddr *faddr);
  ~ClientHandler();

  int noop();
  // Performs clear text I/O
  int read_clear();
  int write_clear();
  // Specialized for PROXY-protocol use; peek data from socket.
  int proxy_protocol_peek_clear();
  // Performs TLS handshake
  int tls_handshake();
  // Performs TLS I/O
  int read_tls();
  int write_tls();

  int upstream_noop();
  int upstream_read();
  int upstream_http2_connhd_read();
  int upstream_http1_connhd_read();
  int upstream_write();

  int proxy_protocol_read();
  int proxy_protocol_v2_read();
  int on_proxy_protocol_finish();

  // Performs I/O operation.  Internally calls on_read()/on_write().
  int do_read();
  int do_write();

  // Processes buffers.  No underlying I/O operation will be done.
  int on_read();
  int on_write();

  struct ev_loop *get_loop() const;
  void reset_upstream_read_timeout(ev_tstamp t);
  void reset_upstream_write_timeout(ev_tstamp t);

  int validate_next_proto();
  const StringRef &get_ipaddr() const;
  bool get_should_close_after_write() const;
  void set_should_close_after_write(bool f);
  Upstream *get_upstream();

  void pool_downstream_connection(std::unique_ptr<DownstreamConnection> dconn);
  void remove_downstream_connection(DownstreamConnection *dconn);
  DownstreamAddr *get_downstream_addr(int &err, DownstreamAddrGroup *group,
                                      Downstream *downstream);
  // Returns DownstreamConnection object based on request path.  This
  // function returns non-null DownstreamConnection, and assigns 0 to
  // |err| if it succeeds, or returns nullptr, and assigns negative
  // error code to |err|.
  std::unique_ptr<DownstreamConnection>
  get_downstream_connection(int &err, Downstream *downstream);
  MemchunkPool *get_mcpool();
  SSL *get_ssl() const;
  // Call this function when HTTP/2 connection header is received at
  // the start of the connection.
  void direct_http2_upgrade();
  // Performs HTTP/2 Upgrade from the connection managed by
  // |http|. If this function fails, the connection must be
  // terminated. This function returns 0 if it succeeds, or -1.
  int perform_http2_upgrade(HttpsUpstream *http);
  bool get_http2_upgrade_allowed() const;
  // Returns upstream scheme, either "http" or "https"
  StringRef get_upstream_scheme() const;
  void start_immediate_shutdown();

  // Writes upstream accesslog using |downstream|.  The |downstream|
  // must not be nullptr.
  void write_accesslog(Downstream *downstream);

  Worker *get_worker() const;

  // Initializes forwarded_for_.
  void init_forwarded_for(int family, const StringRef &ipaddr);

  using ReadBuf = DefaultMemchunkBuffer;

  ReadBuf *get_rb();

  RateLimit *get_rlimit();
  RateLimit *get_wlimit();

  void signal_write();
  ev_io *get_wev();

  void setup_upstream_io_callback();

#ifdef ENABLE_HTTP3
  void setup_http3_upstream(std::unique_ptr<Http3Upstream> &&upstream);
  int read_quic(const UpstreamAddr *faddr, const Address &remote_addr,
                const Address &local_addr, const ngtcp2_pkt_info &pi,
                std::span<const uint8_t> data);
  int write_quic();
#endif // ENABLE_HTTP3

  // Returns string suitable for use in "by" parameter of Forwarded
  // header field.
  StringRef get_forwarded_by() const;
  // Returns string suitable for use in "for" parameter of Forwarded
  // header field.
  StringRef get_forwarded_for() const;

  Http2Session *
  get_http2_session(const std::shared_ptr<DownstreamAddrGroup> &group,
                    DownstreamAddr *addr);

  // Returns an affinity cookie value for |downstream|.  |cookie_name|
  // is used to inspect cookie header field in request header fields.
  uint32_t get_affinity_cookie(Downstream *downstream,
                               const StringRef &cookie_name);

  DownstreamAddr *get_downstream_addr_strict_affinity(
    int &err, const std::shared_ptr<SharedDownstreamAddr> &shared_addr,
    Downstream *downstream);

  const UpstreamAddr *get_upstream_addr() const;

  void repeat_read_timer();
  void stop_read_timer();

  Connection *get_connection();

  // Stores |sni| which is TLS SNI extension value client sent in this
  // connection.
  void set_tls_sni(const StringRef &sni);
  // Returns TLS SNI extension value client sent in this connection.
  StringRef get_tls_sni() const;

  // Returns ALPN negotiated in this connection.
  StringRef get_alpn() const;

  BlockAllocator &get_block_allocator();

  void set_alpn_from_conn();

private:
  // Allocator to allocate memory for connection-wide objects.  Make
  // sure that the allocations must be bounded, and not proportional
  // to the number of requests.
  BlockAllocator balloc_;
  DefaultMemchunkBuffer rb_;
  Connection conn_;
  ev_timer reneg_shutdown_timer_;
  std::unique_ptr<Upstream> upstream_;
  // IP address of client.  If UNIX domain socket is used, this is
  // "localhost".
  StringRef ipaddr_;
  StringRef port_;
  // The ALPN identifier negotiated for this connection.
  StringRef alpn_;
  // The client address used in "for" parameter of Forwarded header
  // field.
  StringRef forwarded_for_;
  // lowercased TLS SNI which client sent.
  StringRef sni_;
  std::function<int(ClientHandler &)> read_, write_;
  std::function<int(ClientHandler &)> on_read_, on_write_;
  // Address of frontend listening socket
  const UpstreamAddr *faddr_;
  Worker *worker_;
  // The number of bytes of HTTP/2 client connection header to read
  size_t left_connhd_len_;
  // hash for session affinity using client IP
  uint32_t affinity_hash_;
  bool should_close_after_write_;
  // true if affinity_hash_ is computed
  bool affinity_hash_computed_;
};

} // namespace shrpx

#endif // SHRPX_CLIENT_HANDLER_H
