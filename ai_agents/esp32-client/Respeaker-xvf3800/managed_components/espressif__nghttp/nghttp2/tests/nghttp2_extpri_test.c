/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2022 nghttp3 contributors
 * Copyright (c) 2022 nghttp2 contributors
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
#include "nghttp2_extpri_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_extpri.h"
#include "nghttp2_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_extpri_to_uint8),
  munit_test_end(),
};

const MunitSuite extpri_suite = {
  "/extpri", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_extpri_to_uint8(void) {
  {
    nghttp2_extpri pri = {1, 0};
    assert_uint8(1, ==, nghttp2_extpri_to_uint8(&pri));
  }
  {
    nghttp2_extpri pri = {1, 1};
    assert_uint8((0x80 | 1), ==, nghttp2_extpri_to_uint8(&pri));
  }
  {
    nghttp2_extpri pri = {7, 1};
    assert_uint8((0x80 | 7), ==, nghttp2_extpri_to_uint8(&pri));
  }
  {
    nghttp2_extpri pri = {7, 0};
    assert_uint8(7, ==, nghttp2_extpri_to_uint8(&pri));
  }
}
