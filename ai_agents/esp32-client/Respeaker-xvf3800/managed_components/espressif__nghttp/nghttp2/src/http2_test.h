/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#ifndef SHRPX_HTTP2_TEST_H
#define SHRPX_HTTP2_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

namespace shrpx {

extern const MunitSuite http2_suite;

munit_void_test_decl(test_http2_add_header)
munit_void_test_decl(test_http2_get_header)
munit_void_test_decl(test_http2_copy_headers_to_nva)
munit_void_test_decl(test_http2_build_http1_headers_from_headers)
munit_void_test_decl(test_http2_lws)
munit_void_test_decl(test_http2_rewrite_location_uri)
munit_void_test_decl(test_http2_parse_http_status_code)
munit_void_test_decl(test_http2_index_header)
munit_void_test_decl(test_http2_lookup_token)
munit_void_test_decl(test_http2_parse_link_header)
munit_void_test_decl(test_http2_path_join)
munit_void_test_decl(test_http2_normalize_path)
munit_void_test_decl(test_http2_rewrite_clean_path)
munit_void_test_decl(test_http2_get_pure_path_component)
munit_void_test_decl(test_http2_construct_push_component)
munit_void_test_decl(test_http2_contains_trailers)
munit_void_test_decl(test_http2_check_transfer_encoding)

} // namespace shrpx

#endif // SHRPX_HTTP2_TEST_H
