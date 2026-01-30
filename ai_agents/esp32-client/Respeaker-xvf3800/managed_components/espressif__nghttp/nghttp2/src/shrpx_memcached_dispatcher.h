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
#ifndef SHRPX_MEMCACHED_DISPATCHER_H
#define SHRPX_MEMCACHED_DISPATCHER_H

#include "shrpx.h"

#include <memory>
#include <random>

#include <ev.h>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include "memchunk.h"
#include "network.h"

using namespace nghttp2;

namespace shrpx {

struct MemcachedRequest;
class MemcachedConnection;

class MemcachedDispatcher {
public:
  MemcachedDispatcher(const Address *addr, struct ev_loop *loop,
                      SSL_CTX *ssl_ctx, const StringRef &sni_name,
                      MemchunkPool *mcpool, std::mt19937 &gen);
  ~MemcachedDispatcher();

  int add_request(std::unique_ptr<MemcachedRequest> req);

private:
  struct ev_loop *loop_;
  std::unique_ptr<MemcachedConnection> mconn_;
};

} // namespace shrpx

#endif // SHRPX_MEMCACHED_DISPATCHER_H
