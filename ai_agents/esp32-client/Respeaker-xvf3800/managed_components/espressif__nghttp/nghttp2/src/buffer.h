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
#ifndef BUFFER_H
#define BUFFER_H

#include "nghttp2_config.h"

#include <cstring>
#include <algorithm>
#include <array>

namespace nghttp2 {

template <size_t N> struct Buffer {
  Buffer() : pos(std::begin(buf)), last(pos) {}
  // Returns the number of bytes to read.
  size_t rleft() const { return last - pos; }
  // Returns the number of bytes this buffer can store.
  size_t wleft() const { return std::end(buf) - last; }
  // Writes up to min(wleft(), |count|) bytes from buffer pointed by
  // |src|.  Returns number of bytes written.
  size_t write(const void *src, size_t count) {
    count = std::min(count, wleft());
    auto p = static_cast<const uint8_t *>(src);
    last = std::copy_n(p, count, last);
    return count;
  }
  size_t write(size_t count) {
    count = std::min(count, wleft());
    last += count;
    return count;
  }
  // Drains min(rleft(), |count|) bytes from start of the buffer.
  size_t drain(size_t count) {
    count = std::min(count, rleft());
    pos += count;
    return count;
  }
  size_t drain_reset(size_t count) {
    count = std::min(count, rleft());
    std::copy(pos + count, last, std::begin(buf));
    last = std::begin(buf) + (last - (pos + count));
    pos = std::begin(buf);
    return count;
  }
  void reset() { pos = last = std::begin(buf); }
  uint8_t *begin() { return std::begin(buf); }
  uint8_t &operator[](size_t n) { return buf[n]; }
  const uint8_t &operator[](size_t n) const { return buf[n]; }
  std::array<uint8_t, N> buf;
  uint8_t *pos, *last;
};

} // namespace nghttp2

#endif // BUFFER_H
