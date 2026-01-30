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
#ifndef SHRPX_LIVE_CHECK_H
#define SHRPX_LIVE_CHECK_H

#include "shrpx.h"

#include <functional>
#include <random>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#include <nghttp2/nghttp2.h>

#include "shrpx_connection.h"

namespace shrpx {

class Worker;
struct DownstreamAddr;
struct DNSQuery;

class LiveCheck {
public:
  LiveCheck(struct ev_loop *loop, SSL_CTX *ssl_ctx, Worker *worker,
            DownstreamAddr *addr, std::mt19937 &gen);
  ~LiveCheck();

  void disconnect();

  void on_success();
  void on_failure();

  int initiate_connection();

  // Schedules next connection attempt
  void schedule();

  // Low level I/O operation callback; they are called from do_read()
  // or do_write().
  int noop();
  int connected();
  int tls_handshake();
  int read_tls();
  int write_tls();
  int read_clear();
  int write_clear();

  int do_read();
  int do_write();

  // These functions are used to feed / extract data to
  // nghttp2_session object.
  int on_read(const uint8_t *data, size_t len);
  int on_write();

  // Call this function when HTTP/2 connection was established.  We
  // don't call this function for HTTP/1 at the moment.
  int connection_made();

  void start_settings_timer();
  void stop_settings_timer();

  // Call this function when SETTINGS ACK was received from server.
  void settings_ack_received();

  void signal_write();

private:
  Connection conn_;
  DefaultMemchunks wb_;
  std::mt19937 &gen_;
  ev_timer backoff_timer_;
  ev_timer settings_timer_;
  std::function<int(LiveCheck &)> read_, write_;
  Worker *worker_;
  // nullptr if no TLS is configured
  SSL_CTX *ssl_ctx_;
  // Address of remote endpoint
  DownstreamAddr *addr_;
  nghttp2_session *session_;
  // Actual remote address used to contact backend.  This is initially
  // nullptr, and may point to either &addr_->addr, or
  // resolved_addr_.get().
  const Address *raddr_;
  // Resolved IP address if dns parameter is used
  std::unique_ptr<Address> resolved_addr_;
  std::unique_ptr<DNSQuery> dns_query_;
  // The number of successful connect attempt in a row.
  size_t success_count_;
  // The number of unsuccessful connect attempt in a row.
  size_t fail_count_;
  // true when SETTINGS ACK has been received from server.
  bool settings_ack_received_;
  // true when GOAWAY has been queued.
  bool session_closing_;
};

} // namespace shrpx

#endif // SHRPX_LIVE_CHECK_H
