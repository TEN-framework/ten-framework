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
#ifndef SHRPX_RATE_LIMIT_H
#define SHRPX_RATE_LIMIT_H

#include "shrpx.h"

#include <ev.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

namespace shrpx {

struct Connection;

class RateLimit {
public:
  // We need |conn| object to check that it has unread bytes for TLS
  // connection.
  RateLimit(struct ev_loop *loop, ev_io *w, size_t rate, size_t burst,
            Connection *conn = nullptr);
  ~RateLimit();
  size_t avail() const;
  void drain(size_t n);
  void regen();
  void startw();
  void stopw();
  // Feeds event if conn_->tls object has unread bytes.  This is
  // required since it is buffered in conn_->tls object, io event is
  // not generated unless new incoming data is received.
  void handle_tls_pending_read();

private:
  ev_timer t_;
  ev_io *w_;
  struct ev_loop *loop_;
  Connection *conn_;
  size_t rate_;
  size_t burst_;
  size_t avail_;
  bool startw_req_;
};

} // namespace shrpx

#endif // SHRPX_RATE_LIMIT_H
