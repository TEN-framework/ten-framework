/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2021 Tatsuhiro Tsujikawa
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
#ifndef HTTP3_H
#define HTTP3_H

#include "nghttp2_config.h"

#include <cstring>
#include <string>
#include <vector>

#include <nghttp3/nghttp3.h>

#include "http2.h"
#include "template.h"

namespace nghttp2 {

namespace http3 {

// Create nghttp3_nv from |name|, |value| and |flags|.
inline nghttp3_nv make_field_flags(const StringRef &name,
                                   const StringRef &value,
                                   uint8_t flags = NGHTTP3_NV_FLAG_NONE) {
  auto ns = as_uint8_span(std::span{name});
  auto vs = as_uint8_span(std::span{value});

  return {const_cast<uint8_t *>(ns.data()), const_cast<uint8_t *>(vs.data()),
          ns.size(), vs.size(), flags};
}

// Creates nghttp3_nv from |name|, |value| and |flags|.  nghttp3
// library does not copy them.
inline nghttp3_nv make_field(const StringRef &name, const StringRef &value,
                             uint8_t flags = NGHTTP3_NV_FLAG_NONE) {
  return make_field_flags(name, value,
                          static_cast<uint8_t>(NGHTTP3_NV_FLAG_NO_COPY_NAME |
                                               NGHTTP3_NV_FLAG_NO_COPY_VALUE |
                                               flags));
}

// Returns NGHTTP3_NV_FLAG_NEVER_INDEX if |never_index| is true,
// otherwise NGHTTP3_NV_FLAG_NONE.
inline uint8_t never_index(bool never_index) {
  return never_index ? NGHTTP3_NV_FLAG_NEVER_INDEX : NGHTTP3_NV_FLAG_NONE;
}

// Just like copy_headers_to_nva(), but this adds
// NGHTTP3_NV_FLAG_NO_COPY_NAME and NGHTTP3_NV_FLAG_NO_COPY_VALUE.
void copy_headers_to_nva_nocopy(std::vector<nghttp3_nv> &nva,
                                const HeaderRefs &headers, uint32_t flags);

} // namespace http3

} // namespace nghttp2

#endif // HTTP3_H
