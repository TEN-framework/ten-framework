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
#include "nghttp2_helper_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_adjust_local_window_size),
  munit_void_test(test_nghttp2_check_header_name),
  munit_void_test(test_nghttp2_check_header_value),
  munit_void_test(test_nghttp2_check_header_value_rfc9113),
  munit_test_end(),
};

const MunitSuite helper_suite = {
  "/helper", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_adjust_local_window_size(void) {
  int32_t local_window_size = 100;
  int32_t recv_window_size = 50;
  int32_t recv_reduction = 0;
  int32_t delta;

  delta = 0;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(50, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(0, ==, delta);

  delta = 49;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(1, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(49, ==, delta);

  delta = 1;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(0, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(1, ==, delta);

  delta = 1;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(101, ==, local_window_size);
  assert_int32(0, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(1, ==, delta);

  delta = -1;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(-1, ==, recv_window_size);
  assert_int32(1, ==, recv_reduction);
  assert_int32(0, ==, delta);

  delta = 1;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(101, ==, local_window_size);
  assert_int32(0, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(0, ==, delta);

  delta = 100;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(201, ==, local_window_size);
  assert_int32(0, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(100, ==, delta);

  delta = -3;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(198, ==, local_window_size);
  assert_int32(-3, ==, recv_window_size);
  assert_int32(3, ==, recv_reduction);
  assert_int32(0, ==, delta);

  recv_window_size += 3;

  delta = 3;
  assert_int(0, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(201, ==, local_window_size);
  assert_int32(3, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(0, ==, delta);

  local_window_size = 100;
  recv_window_size = 50;
  recv_reduction = 0;
  delta = INT32_MAX;
  assert_int(NGHTTP2_ERR_FLOW_CONTROL, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(50, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(INT32_MAX, ==, delta);

  delta = INT32_MIN;
  assert_int(NGHTTP2_ERR_FLOW_CONTROL, ==,
             nghttp2_adjust_local_window_size(
               &local_window_size, &recv_window_size, &recv_reduction, &delta));
  assert_int32(100, ==, local_window_size);
  assert_int32(50, ==, recv_window_size);
  assert_int32(0, ==, recv_reduction);
  assert_int32(INT32_MIN, ==, delta);
}

#define check_header_name(S)                                                   \
  nghttp2_check_header_name((const uint8_t *)S, sizeof(S) - 1)

void test_nghttp2_check_header_name(void) {
  assert_true(check_header_name(":path"));
  assert_true(check_header_name("path"));
  assert_true(check_header_name("!#$%&'*+-.^_`|~"));
  assert_false(check_header_name(":PATH"));
  assert_false(check_header_name("path:"));
  assert_false(check_header_name(""));
  assert_false(check_header_name(":"));
}

#define check_header_value(S)                                                  \
  nghttp2_check_header_value((const uint8_t *)S, sizeof(S) - 1)

void test_nghttp2_check_header_value(void) {
  uint8_t goodval[] = {'a', 'b', 0x80u, 'c', 0xffu, 'd', '\t', ' '};
  uint8_t badval1[] = {'a', 0x1fu, 'b'};
  uint8_t badval2[] = {'a', 0x7fu, 'b'};

  assert_true(check_header_value(" !|}~"));
  assert_true(check_header_value(goodval));
  assert_false(check_header_value(badval1));
  assert_false(check_header_value(badval2));
  assert_true(check_header_value(""));
  assert_true(check_header_value(" "));
  assert_true(check_header_value("\t"));
}

#define check_header_value_rfc9113(S)                                          \
  nghttp2_check_header_value_rfc9113((const uint8_t *)S, sizeof(S) - 1)

void test_nghttp2_check_header_value_rfc9113(void) {
  uint8_t goodval[] = {'a', 'b', 0x80u, 'c', 0xffu, 'd'};
  uint8_t badval1[] = {'a', 0x1fu, 'b'};
  uint8_t badval2[] = {'a', 0x7fu, 'b'};

  assert_true(check_header_value_rfc9113("!|}~"));
  assert_false(check_header_value_rfc9113(" !|}~"));
  assert_false(check_header_value_rfc9113("!|}~ "));
  assert_false(check_header_value_rfc9113("\t!|}~"));
  assert_false(check_header_value_rfc9113("!|}~\t"));
  assert_true(check_header_value_rfc9113(goodval));
  assert_false(check_header_value_rfc9113(badval1));
  assert_false(check_header_value_rfc9113(badval2));
  assert_true(check_header_value_rfc9113(""));
  assert_false(check_header_value_rfc9113(" "));
  assert_false(check_header_value_rfc9113("\t"));
}
