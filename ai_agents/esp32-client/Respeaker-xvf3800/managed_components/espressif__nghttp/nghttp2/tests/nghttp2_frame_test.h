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
#ifndef NGHTTP2_FRAME_TEST_H
#define NGHTTP2_FRAME_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

extern const MunitSuite frame_suite;

munit_void_test_decl(test_nghttp2_frame_pack_headers)
munit_void_test_decl(test_nghttp2_frame_pack_headers_frame_too_large)
munit_void_test_decl(test_nghttp2_frame_pack_priority)
munit_void_test_decl(test_nghttp2_frame_pack_rst_stream)
munit_void_test_decl(test_nghttp2_frame_pack_settings)
munit_void_test_decl(test_nghttp2_frame_pack_push_promise)
munit_void_test_decl(test_nghttp2_frame_pack_ping)
munit_void_test_decl(test_nghttp2_frame_pack_goaway)
munit_void_test_decl(test_nghttp2_frame_pack_window_update)
munit_void_test_decl(test_nghttp2_frame_pack_altsvc)
munit_void_test_decl(test_nghttp2_frame_pack_origin)
munit_void_test_decl(test_nghttp2_frame_pack_priority_update)
munit_void_test_decl(test_nghttp2_nv_array_copy)
munit_void_test_decl(test_nghttp2_iv_check)

#endif /* NGHTTP2_FRAME_TEST_H */
