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
#ifndef SHRPX_CONNECTION_H
#define SHRPX_CONNECTION_H

#include "shrpx_config.h"

#include <sys/uio.h>

#include <ev.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <nghttp2/nghttp2.h>

#ifdef ENABLE_HTTP3
#  include <ngtcp2/ngtcp2_crypto.h>
#endif // ENABLE_HTTP3

#include "shrpx_rate_limit.h"
#include "shrpx_error.h"
#include "memchunk.h"

namespace shrpx {

struct MemcachedRequest;

namespace tls {
struct TLSSessionCache;
} // namespace tls

enum class TLSHandshakeState {
  NORMAL,
  WAIT_FOR_SESSION_CACHE,
  GOT_SESSION_CACHE,
  CANCEL_SESSION_CACHE,
  WRITE_STARTED,
};

struct TLSConnection {
  DefaultMemchunks wbuf;
  DefaultPeekMemchunks rbuf;
  // Stores TLSv1.3 early data.
  DefaultMemchunks earlybuf;
  SSL *ssl;
  SSL_SESSION *cached_session;
  MemcachedRequest *cached_session_lookup_req;
  tls::TLSSessionCache *client_session_cache;
  std::chrono::steady_clock::time_point last_write_idle;
  size_t warmup_writelen;
  // length passed to SSL_write and SSL_read last time.  This is
  // required since these functions require the exact same parameters
  // on non-blocking I/O.
  size_t last_writelen, last_readlen;
  TLSHandshakeState handshake_state;
  bool initial_handshake_done;
  bool reneg_started;
  // true if ssl is prepared to do handshake as server.
  bool server_handshake;
  // true if ssl is initialized as server, and client requested
  // signed_certificate_timestamp extension.
  bool sct_requested;
  // true if TLSv1.3 early data has been completely received.  Since
  // SSL_read_early_data acts like SSL_do_handshake, this field may be
  // true even if the negotiated TLS version is TLSv1.2 or earlier.
  // This value is also true if this is client side connection for
  // convenience.
  bool early_data_finish;
};

struct TCPHint {
  size_t write_buffer_size;
  uint32_t rwin;
};

template <typename T> using EVCb = void (*)(struct ev_loop *, T *, int);

using IOCb = EVCb<ev_io>;
using TimerCb = EVCb<ev_timer>;

struct Connection {
  Connection(struct ev_loop *loop, int fd, SSL *ssl, MemchunkPool *mcpool,
             ev_tstamp write_timeout, ev_tstamp read_timeout,
             const RateLimitConfig &write_limit,
             const RateLimitConfig &read_limit, IOCb writecb, IOCb readcb,
             TimerCb timeoutcb, void *data, size_t tls_dyn_rec_warmup_threshold,
             ev_tstamp tls_dyn_rec_idle_timeout, Proto proto);
  ~Connection();

  void disconnect();

  void prepare_client_handshake();
  void prepare_server_handshake();

  int tls_handshake();
  int tls_handshake_simple();
  int write_tls_pending_handshake();

  int check_http2_requirement();

  // All write_* and writev_clear functions return number of bytes
  // written.  If nothing cannot be written (e.g., there is no
  // allowance in RateLimit or underlying connection blocks), return
  // 0.  SHRPX_ERR_NETWORK is returned in case of error.
  //
  // All read_* functions return number of bytes read.  If nothing
  // cannot be read (e.g., there is no allowance in Ratelimit or
  // underlying connection blocks), return 0.  SHRPX_ERR_EOF is
  // returned in case of EOF and no data was read.  Otherwise
  // SHRPX_ERR_NETWORK is return in case of error.
  nghttp2_ssize write_tls(const void *data, size_t len);
  nghttp2_ssize read_tls(void *data, size_t len);

  size_t get_tls_write_limit();
  // Updates the number of bytes written in warm up period.
  void update_tls_warmup_writelen(size_t n);
  // Tells there is no immediate write now.  This triggers timer to
  // determine fallback to short record size mode.
  void start_tls_write_idle();

  nghttp2_ssize write_clear(const void *data, size_t len);
  nghttp2_ssize writev_clear(struct iovec *iov, int iovcnt);
  nghttp2_ssize read_clear(void *data, size_t len);
  // Read at most |len| bytes of data from socket without rate limit.
  nghttp2_ssize read_nolim_clear(void *data, size_t len);
  // Peek at most |len| bytes of data from socket without rate limit.
  nghttp2_ssize peek_clear(void *data, size_t len);

  void handle_tls_pending_read();

  void set_ssl(SSL *ssl);

  int get_tcp_hint(TCPHint *hint) const;

  // These functions are provided for read timer which is frequently
  // restarted.  We do a trick to make a bit more efficient than just
  // calling ev_timer_again().

  // Restarts read timer with timeout value |t|.
  void again_rt(ev_tstamp t);
  // Restarts read timer without changing timeout.
  void again_rt();
  // Returns true if read timer expired.
  bool expired_rt();

#ifdef ENABLE_HTTP3
  // This must be the first member of Connection.
  ngtcp2_crypto_conn_ref conn_ref;
#endif // ENABLE_HTTP3
  TLSConnection tls;
  ev_io wev;
  ev_io rev;
  ev_timer wt;
  ev_timer rt;
  RateLimit wlimit;
  RateLimit rlimit;
  struct ev_loop *loop;
  void *data;
  int fd;
  size_t tls_dyn_rec_warmup_threshold;
  std::chrono::steady_clock::duration tls_dyn_rec_idle_timeout;
  // Application protocol used over the connection.  This field is not
  // used in this object at the moment.  The rest of the program may
  // use this value when it is useful.
  Proto proto;
  // The point of time when last read is observed.  Note: since we use
  // |rt| as idle timer, the activity is not limited to read.
  std::chrono::steady_clock::time_point last_read;
  // Timeout for read timer |rt|.
  ev_tstamp read_timeout;
};

#ifdef ENABLE_HTTP3
static_assert(std::is_standard_layout<Connection>::value,
              "Connection is not standard layout");
#endif // ENABLE_HTTP3

// Creates BIO_method shared by all SSL objects.
BIO_METHOD *create_bio_method();

} // namespace shrpx

#endif // SHRPX_CONNECTION_H
