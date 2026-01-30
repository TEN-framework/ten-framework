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
#include "nghttp2_queue_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_queue.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_queue),
  munit_test_end(),
};

const MunitSuite queue_suite = {
  "/queue", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_queue(void) {
  int ints[] = {1, 2, 3, 4, 5};
  int i;
  nghttp2_queue queue;
  nghttp2_queue_init(&queue);
  assert_true(nghttp2_queue_empty(&queue));
  for (i = 0; i < 5; ++i) {
    nghttp2_queue_push(&queue, &ints[i]);
    assert_int(ints[0], ==, *(int *)(nghttp2_queue_front(&queue)));
    assert_false(nghttp2_queue_empty(&queue));
  }
  for (i = 0; i < 5; ++i) {
    assert_int(ints[i], ==, *(int *)(nghttp2_queue_front(&queue)));
    nghttp2_queue_pop(&queue);
  }
  assert_true(nghttp2_queue_empty(&queue));
  nghttp2_queue_free(&queue);
}
