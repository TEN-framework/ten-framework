/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#ifndef H2LOAD_SESSION_H
#define H2LOAD_SESSION_H

#include "nghttp2_config.h"

#include <sys/types.h>

#include <cinttypes>

#include "h2load.h"

namespace h2load {

class Session {
public:
  virtual ~Session() {}
  // Called when the connection was made.
  virtual void on_connect() = 0;
  // Called when one request must be issued.
  virtual int submit_request() = 0;
  // Called when incoming bytes are available. The subclass has to
  // return the number of bytes read.
  virtual int on_read(const uint8_t *data, size_t len) = 0;
  // Called when write is available. Returns 0 on success, otherwise
  // return -1.
  virtual int on_write() = 0;
  // Called when the underlying session must be terminated.
  virtual void terminate() = 0;
  // Return the maximum concurrency per connection
  virtual size_t max_concurrent_streams() = 0;
};

} // namespace h2load

#endif // H2LOAD_SESSION_H
