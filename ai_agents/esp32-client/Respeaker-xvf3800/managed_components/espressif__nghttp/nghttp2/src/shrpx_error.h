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
#ifndef SHRPX_ERROR_H
#define SHRPX_ERROR_H

#include "shrpx.h"

namespace shrpx {

// Deprecated, do not use.
enum ErrorCode {
  SHRPX_ERR_SUCCESS = 0,
  SHRPX_ERR_ERROR = -1,
  SHRPX_ERR_NETWORK = -100,
  SHRPX_ERR_EOF = -101,
  SHRPX_ERR_INPROGRESS = -102,
  SHRPX_ERR_DCONN_CANCELED = -103,
  SHRPX_ERR_RETRY = -104,
  SHRPX_ERR_TLS_REQUIRED = -105,
  SHRPX_ERR_SEND_BLOCKED = -106,
};

} // namespace shrpx

#endif // SHRPX_ERROR_H
