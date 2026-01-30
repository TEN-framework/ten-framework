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
#ifndef SHRPX_HTTP_H
#define SHRPX_HTTP_H

#include "shrpx.h"

#include <string>

#include <nghttp2/nghttp2.h>

#include "shrpx_config.h"
#include "util.h"
#include "allocator.h"

using namespace nghttp2;

namespace shrpx {

namespace http {

StringRef create_error_html(BlockAllocator &balloc, unsigned int status_code);

template <typename OutputIt>
OutputIt create_via_header_value(OutputIt dst, int major, int minor) {
  *dst++ = static_cast<char>(major + '0');
  if (major < 2) {
    *dst++ = '.';
    *dst++ = static_cast<char>(minor + '0');
  }
  return util::copy_lit(dst, " nghttpx");
}

// Returns generated RFC 7239 Forwarded header field value.  The
// |params| is bitwise-OR of zero or more of shrpx_forwarded_param
// defined in shrpx_config.h.
StringRef create_forwarded(BlockAllocator &balloc, int params,
                           const StringRef &node_by, const StringRef &node_for,
                           const StringRef &host, const StringRef &proto);

// Adds ANSI color codes to HTTP headers |hdrs|.
std::string colorizeHeaders(const char *hdrs);

nghttp2_ssize select_padding_callback(nghttp2_session *session,
                                      const nghttp2_frame *frame,
                                      size_t max_payload, void *user_data);

// Creates set-cookie-string for cookie based affinity.  If |path| is
// not empty, "; <path>" is added.  If |secure| is true, "; Secure" is
// added.
StringRef create_affinity_cookie(BlockAllocator &balloc, const StringRef &name,
                                 uint32_t affinity_cookie,
                                 const StringRef &path, bool secure);

// Returns true if |secure| indicates that Secure attribute should be
// set.
bool require_cookie_secure_attribute(SessionAffinityCookieSecure secure,
                                     const StringRef &scheme);

// Returns RFC 7838 alt-svc header field value.
StringRef create_altsvc_header_value(BlockAllocator &balloc,
                                     const std::vector<AltSvc> &altsvcs);

// Returns true if either of the following conditions holds:
// - scheme is https and encrypted is true
// - scheme is http and encrypted is false
// Otherwise returns false.
bool check_http_scheme(const StringRef &scheme, bool encrypted);

} // namespace http

} // namespace shrpx

#endif // SHRPX_HTTP_H
