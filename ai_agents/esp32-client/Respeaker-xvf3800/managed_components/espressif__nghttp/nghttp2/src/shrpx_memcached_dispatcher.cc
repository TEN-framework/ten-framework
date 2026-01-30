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
#include "shrpx_memcached_dispatcher.h"

#include "shrpx_memcached_request.h"
#include "shrpx_memcached_connection.h"
#include "shrpx_config.h"
#include "shrpx_log.h"

namespace shrpx {

MemcachedDispatcher::MemcachedDispatcher(const Address *addr,
                                         struct ev_loop *loop, SSL_CTX *ssl_ctx,
                                         const StringRef &sni_name,
                                         MemchunkPool *mcpool,
                                         std::mt19937 &gen)
  : loop_(loop),
    mconn_(std::make_unique<MemcachedConnection>(addr, loop_, ssl_ctx, sni_name,
                                                 mcpool, gen)) {}

MemcachedDispatcher::~MemcachedDispatcher() {}

int MemcachedDispatcher::add_request(std::unique_ptr<MemcachedRequest> req) {
  if (mconn_->add_request(std::move(req)) != 0) {
    return -1;
  }

  return 0;
}

} // namespace shrpx
