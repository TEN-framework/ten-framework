/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#include "nghttp2_buf_test.h"

#include <stdio.h>

#include "munit.h"

#include "nghttp2_buf.h"
#include "nghttp2_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_bufs_add),
  munit_void_test(test_nghttp2_bufs_add_stack_buffer_overflow_bug),
  munit_void_test(test_nghttp2_bufs_addb),
  munit_void_test(test_nghttp2_bufs_orb),
  munit_void_test(test_nghttp2_bufs_remove),
  munit_void_test(test_nghttp2_bufs_reset),
  munit_void_test(test_nghttp2_bufs_advance),
  munit_void_test(test_nghttp2_bufs_next_present),
  munit_void_test(test_nghttp2_bufs_realloc),
  munit_test_end(),
};

const MunitSuite buf_suite = {
  "/buf", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_bufs_add(void) {
  int rv;
  nghttp2_bufs bufs;
  uint8_t data[2048];
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 1000, 3, mem);
  assert_int(0, ==, rv);

  assert_ptr_equal(bufs.cur->buf.pos, bufs.cur->buf.last);

  rv = nghttp2_bufs_add(&bufs, data, 493);
  assert_int(0, ==, rv);
  assert_size(493, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(493, ==, nghttp2_bufs_len(&bufs));
  assert_size(507, ==, nghttp2_bufs_cur_avail(&bufs));

  rv = nghttp2_bufs_add(&bufs, data, 507);
  assert_int(0, ==, rv);
  assert_size(1000, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1000, ==, nghttp2_bufs_len(&bufs));
  assert_ptr_equal(bufs.cur, bufs.head);

  rv = nghttp2_bufs_add(&bufs, data, 1);
  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1001, ==, nghttp2_bufs_len(&bufs));
  assert_ptr_equal(bufs.cur, bufs.head->next);

  nghttp2_bufs_free(&bufs);
}

/* Test for GH-232, stack-buffer-overflow */
void test_nghttp2_bufs_add_stack_buffer_overflow_bug(void) {
  int rv;
  nghttp2_bufs bufs;
  uint8_t data[1024];
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 100, 200, mem);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_add(&bufs, data, sizeof(data));

  assert_int(0, ==, rv);
  assert_size(sizeof(data), ==, nghttp2_bufs_len(&bufs));

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_addb(void) {
  int rv;
  nghttp2_bufs bufs;
  size_t i;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 1000, 3, mem);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_addb(&bufs, 14);
  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(14, ==, *bufs.cur->buf.pos);

  for (i = 0; i < 999; ++i) {
    rv = nghttp2_bufs_addb(&bufs, 254);

    assert_int(0, ==, rv);
    assert_size(i + 2, ==, nghttp2_buf_len(&bufs.cur->buf));
    assert_size(i + 2, ==, nghttp2_bufs_len(&bufs));
    assert_uint8(254, ==, *(bufs.cur->buf.last - 1));
    assert_ptr_equal(bufs.cur, bufs.head);
  }

  rv = nghttp2_bufs_addb(&bufs, 253);
  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1001, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(253, ==, *(bufs.cur->buf.last - 1));
  assert_ptr_equal(bufs.cur, bufs.head->next);

  rv = nghttp2_bufs_addb_hold(&bufs, 15);
  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1001, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(15, ==, *(bufs.cur->buf.last));

  /* test fast version */

  nghttp2_bufs_fast_addb(&bufs, 240);

  assert_size(2, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1002, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(240, ==, *(bufs.cur->buf.last - 1));

  nghttp2_bufs_fast_addb_hold(&bufs, 113);

  assert_size(2, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1002, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(113, ==, *(bufs.cur->buf.last));

  /* addb_hold when last == end */
  bufs.cur->buf.last = bufs.cur->buf.end;

  rv = nghttp2_bufs_addb_hold(&bufs, 19);
  assert_int(0, ==, rv);
  assert_size(0, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(2000, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(19, ==, *(bufs.cur->buf.last));

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_orb(void) {
  int rv;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 1000, 3, mem);
  assert_int(0, ==, rv);

  *(bufs.cur->buf.last) = 0;

  rv = nghttp2_bufs_orb_hold(&bufs, 15);
  assert_int(0, ==, rv);
  assert_size(0, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(0, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(15, ==, *(bufs.cur->buf.last));

  rv = nghttp2_bufs_orb(&bufs, 240);
  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp2_buf_len(&bufs.cur->buf));
  assert_size(1, ==, nghttp2_bufs_len(&bufs));
  assert_uint8(255, ==, *(bufs.cur->buf.last - 1));

  *(bufs.cur->buf.last) = 0;
  nghttp2_bufs_fast_orb_hold(&bufs, 240);
  assert_uint8(240, ==, *(bufs.cur->buf.last));

  nghttp2_bufs_fast_orb(&bufs, 15);
  assert_uint8(255, ==, *(bufs.cur->buf.last - 1));

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_remove(void) {
  int rv;
  nghttp2_bufs bufs;
  nghttp2_buf_chain *chain;
  int i;
  uint8_t *out;
  nghttp2_ssize outlen;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 1000, 3, mem);
  assert_int(0, ==, rv);

  nghttp2_buf_shift_right(&bufs.cur->buf, 10);

  rv = nghttp2_bufs_add(&bufs, "hello ", 6);
  assert_int(0, ==, rv);

  for (i = 0; i < 2; ++i) {
    chain = bufs.cur;

    rv = nghttp2_bufs_advance(&bufs);
    assert_int(0, ==, rv);

    assert_ptr_equal(chain->next, bufs.cur);
  }

  rv = nghttp2_bufs_add(&bufs, "world", 5);
  assert_int(0, ==, rv);

  outlen = nghttp2_bufs_remove(&bufs, &out);
  assert_ptrdiff(11, ==, outlen);

  assert_memory_equal((size_t)outlen, "hello world", out);
  assert_size(11, ==, nghttp2_bufs_len(&bufs));

  mem->free(out, NULL);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_reset(void) {
  int rv;
  nghttp2_bufs bufs;
  nghttp2_buf_chain *ci;
  size_t offset = 9;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init3(&bufs, 250, 3, 1, offset, mem);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_add(&bufs, "foo", 3);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_advance(&bufs);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_add(&bufs, "bar", 3);
  assert_int(0, ==, rv);

  assert_size(6, ==, nghttp2_bufs_len(&bufs));

  nghttp2_bufs_reset(&bufs);

  assert_size(0, ==, nghttp2_bufs_len(&bufs));
  assert_ptr_equal(bufs.cur, bufs.head);

  for (ci = bufs.head; ci; ci = ci->next) {
    assert_ptrdiff((ptrdiff_t)offset, ==, ci->buf.pos - ci->buf.begin);
    assert_ptr_equal(ci->buf.pos, ci->buf.last);
  }

  assert_null(bufs.head->next);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_advance(void) {
  int rv;
  nghttp2_bufs bufs;
  int i;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 250, 3, mem);
  assert_int(0, ==, rv);

  for (i = 0; i < 2; ++i) {
    rv = nghttp2_bufs_advance(&bufs);
    assert_int(0, ==, rv);
  }

  rv = nghttp2_bufs_advance(&bufs);
  assert_int(NGHTTP2_ERR_BUFFER_ERROR, ==, rv);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_next_present(void) {
  int rv;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init(&bufs, 250, 3, mem);
  assert_int(0, ==, rv);

  assert_false(nghttp2_bufs_next_present(&bufs));

  rv = nghttp2_bufs_advance(&bufs);
  assert_int(0, ==, rv);

  nghttp2_bufs_rewind(&bufs);

  assert_false(nghttp2_bufs_next_present(&bufs));

  bufs.cur = bufs.head->next;

  rv = nghttp2_bufs_addb(&bufs, 1);
  assert_int(0, ==, rv);

  nghttp2_bufs_rewind(&bufs);

  assert_true(nghttp2_bufs_next_present(&bufs));

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_bufs_realloc(void) {
  int rv;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  rv = nghttp2_bufs_init3(&bufs, 266, 3, 1, 10, mem);
  assert_int(0, ==, rv);

  /* Create new buffer to see that these buffers are deallocated on
     realloc */
  rv = nghttp2_bufs_advance(&bufs);
  assert_int(0, ==, rv);

  rv = nghttp2_bufs_realloc(&bufs, 522);
  assert_int(0, ==, rv);

  assert_size(512, ==, nghttp2_bufs_cur_avail(&bufs));

  rv = nghttp2_bufs_realloc(&bufs, 9);
  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);

  nghttp2_bufs_free(&bufs);
}
