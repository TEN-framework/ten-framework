/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#include "buffer_test.h"

#include <cstring>
#include <iostream>
#include <tuple>

#include "munitxx.h"

#include <nghttp2/nghttp2.h>

#include "buffer.h"

namespace nghttp2 {

namespace {
const MunitTest tests[]{
  munit_void_test(test_buffer_write),
  munit_test_end(),
};
} // namespace

const MunitSuite buffer_suite{
  "/buffer", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_buffer_write(void) {
  Buffer<16> b;
  assert_size(0, ==, b.rleft());
  assert_size(16, ==, b.wleft());

  b.write("012", 3);

  assert_size(3, ==, b.rleft());
  assert_size(13, ==, b.wleft());
  assert_ptr_equal(b.pos, std::begin(b.buf));

  b.drain(3);

  assert_size(0, ==, b.rleft());
  assert_size(13, ==, b.wleft());
  assert_ptrdiff(3, ==, b.pos - std::begin(b.buf));

  auto n = b.write("0123456789ABCDEF", 16);

  assert_ssize(13, ==, n);

  assert_size(13, ==, b.rleft());
  assert_size(0, ==, b.wleft());
  assert_ptrdiff(3, ==, b.pos - std::begin(b.buf));
  assert_memory_equal(13, b.pos, "0123456789ABC");

  b.reset();

  assert_size(0, ==, b.rleft());
  assert_size(16, ==, b.wleft());
  assert_ptr_equal(b.pos, std::begin(b.buf));

  b.write(5);

  assert_size(5, ==, b.rleft());
  assert_size(11, ==, b.wleft());
  assert_ptr_equal(b.pos, std::begin(b.buf));
}

} // namespace nghttp2
