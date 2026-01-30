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
#ifndef SHRPX_MEMCACHED_RESULT_H
#define SHRPX_MEMCACHED_RESULT_H

#include "shrpx.h"

#include <vector>

namespace shrpx {

enum class MemcachedStatusCode : uint16_t {
  NO_ERROR,
  EXT_NETWORK_ERROR = 0x1001,
};

struct MemcachedResult {
  MemcachedResult(MemcachedStatusCode status_code) : status_code(status_code) {}
  MemcachedResult(MemcachedStatusCode status_code, std::vector<uint8_t> value)
    : value(std::move(value)), status_code(status_code) {}

  std::vector<uint8_t> value;
  MemcachedStatusCode status_code;
};

} // namespace shrpx

#endif // SHRPX_MEMCACHED_RESULT_H
