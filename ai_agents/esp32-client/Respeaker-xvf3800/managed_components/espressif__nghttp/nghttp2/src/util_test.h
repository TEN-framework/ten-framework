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
#ifndef UTIL_TEST_H
#define UTIL_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

namespace shrpx {

extern const MunitSuite util_suite;

munit_void_test_decl(test_util_streq)
munit_void_test_decl(test_util_strieq)
munit_void_test_decl(test_util_inp_strlower)
munit_void_test_decl(test_util_to_base64)
munit_void_test_decl(test_util_to_token68)
munit_void_test_decl(test_util_percent_encode_token)
munit_void_test_decl(test_util_percent_decode)
munit_void_test_decl(test_util_quote_string)
munit_void_test_decl(test_util_utox)
munit_void_test_decl(test_util_http_date)
munit_void_test_decl(test_util_select_h2)
munit_void_test_decl(test_util_ipv6_numeric_addr)
munit_void_test_decl(test_util_utos)
munit_void_test_decl(test_util_make_string_ref_uint)
munit_void_test_decl(test_util_utos_unit)
munit_void_test_decl(test_util_utos_funit)
munit_void_test_decl(test_util_parse_uint_with_unit)
munit_void_test_decl(test_util_parse_uint)
munit_void_test_decl(test_util_parse_duration_with_unit)
munit_void_test_decl(test_util_duration_str)
munit_void_test_decl(test_util_format_duration)
munit_void_test_decl(test_util_starts_with)
munit_void_test_decl(test_util_ends_with)
munit_void_test_decl(test_util_parse_http_date)
munit_void_test_decl(test_util_localtime_date)
munit_void_test_decl(test_util_get_uint64)
munit_void_test_decl(test_util_parse_config_str_list)
munit_void_test_decl(test_util_make_http_hostport)
munit_void_test_decl(test_util_make_hostport)
munit_void_test_decl(test_util_random_alpha_digit)
munit_void_test_decl(test_util_format_hex)
munit_void_test_decl(test_util_is_hex_string)
munit_void_test_decl(test_util_decode_hex)
munit_void_test_decl(test_util_extract_host)
munit_void_test_decl(test_util_split_hostport)
munit_void_test_decl(test_util_split_str)
munit_void_test_decl(test_util_rstrip)

} // namespace shrpx

#endif // UTIL_TEST_H
