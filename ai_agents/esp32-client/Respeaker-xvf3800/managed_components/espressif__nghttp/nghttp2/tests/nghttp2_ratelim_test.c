/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2023 nghttp2 contributors
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
#include "nghttp2_ratelim_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_ratelim.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_ratelim_update),
  munit_void_test(test_nghttp2_ratelim_drain),
  munit_test_end(),
};

const MunitSuite ratelim_suite = {
  "/ratelim", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_ratelim_update(void) {
  nghttp2_ratelim rl;

  nghttp2_ratelim_init(&rl, 1000, 21);

  assert_uint64(1000, ==, rl.val);
  assert_uint64(1000, ==, rl.burst);
  assert_uint64(21, ==, rl.rate);
  assert_uint64(0, ==, rl.tstamp);

  nghttp2_ratelim_update(&rl, 999);

  assert_uint64(1000, ==, rl.val);
  assert_uint64(999, ==, rl.tstamp);

  nghttp2_ratelim_drain(&rl, 100);

  assert_uint64(900, ==, rl.val);

  nghttp2_ratelim_update(&rl, 1000);

  assert_uint64(921, ==, rl.val);

  nghttp2_ratelim_update(&rl, 1002);

  assert_uint64(963, ==, rl.val);

  nghttp2_ratelim_update(&rl, 1004);

  assert_uint64(1000, ==, rl.val);
  assert_uint64(1004, ==, rl.tstamp);

  /* timer skew */
  nghttp2_ratelim_init(&rl, 1000, 21);
  nghttp2_ratelim_update(&rl, 1);

  assert_uint64(1000, ==, rl.val);

  nghttp2_ratelim_update(&rl, 0);

  assert_uint64(1000, ==, rl.val);

  /* rate * duration overflow */
  nghttp2_ratelim_init(&rl, 1000, 100);
  nghttp2_ratelim_drain(&rl, 999);

  assert_uint64(1, ==, rl.val);

  nghttp2_ratelim_update(&rl, UINT64_MAX);

  assert_uint64(1000, ==, rl.val);

  /* val + rate * duration overflow */
  nghttp2_ratelim_init(&rl, UINT64_MAX - 1, 2);
  nghttp2_ratelim_update(&rl, 1);

  assert_uint64(UINT64_MAX - 1, ==, rl.val);
}

void test_nghttp2_ratelim_drain(void) {
  nghttp2_ratelim rl;

  nghttp2_ratelim_init(&rl, 100, 7);

  assert_int(-1, ==, nghttp2_ratelim_drain(&rl, 101));
  assert_int(0, ==, nghttp2_ratelim_drain(&rl, 51));
  assert_int(0, ==, nghttp2_ratelim_drain(&rl, 49));
  assert_int(-1, ==, nghttp2_ratelim_drain(&rl, 1));
}
