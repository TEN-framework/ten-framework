/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Twist Inc.
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
#include "nghttp2_alpn_test.h"

#include <stdio.h>
#include <string.h>

#include "munit.h"

#include <nghttp2/nghttp2.h>

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_alpn),
  munit_test_end(),
};

const MunitSuite alpn_suite = {
  "/alpn", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

static void http2(void) {
  const unsigned char p[] = {8,   'h', 't', 't', 'p', '/', '1', '.', '1', 2,
                             'h', '2', 6,   's', 'p', 'd', 'y', '/', '3'};
  unsigned char outlen;
  const unsigned char *out;
  assert_int(1, ==,
             nghttp2_select_next_protocol((unsigned char **)&out, &outlen, p,
                                          sizeof(p)));
  assert_uchar(NGHTTP2_PROTO_VERSION_ID_LEN, ==, outlen);
  assert_memory_equal(outlen, NGHTTP2_PROTO_VERSION_ID, out);

  outlen = 0;
  out = NULL;

  assert_int(1, ==, nghttp2_select_alpn(&out, &outlen, p, sizeof(p)));
  assert_uchar(NGHTTP2_PROTO_VERSION_ID_LEN, ==, outlen);
  assert_memory_equal(outlen, NGHTTP2_PROTO_VERSION_ID, out);
}

static void http11(void) {
  const unsigned char spdy[] = {
    6,   's', 'p', 'd', 'y', '/', '4', 8,   's', 'p', 'd', 'y', '/',
    '2', '.', '1', 8,   'h', 't', 't', 'p', '/', '1', '.', '1',
  };
  unsigned char outlen;
  const unsigned char *out;
  assert_int(0, ==,
             nghttp2_select_next_protocol((unsigned char **)&out, &outlen, spdy,
                                          sizeof(spdy)));
  assert_uchar(8, ==, outlen);
  assert_memory_equal(outlen, "http/1.1", out);

  outlen = 0;
  out = NULL;

  assert_int(0, ==, nghttp2_select_alpn(&out, &outlen, spdy, sizeof(spdy)));
  assert_uchar(8, ==, outlen);
  assert_memory_equal(outlen, "http/1.1", out);
}

static void no_overlap(void) {
  const unsigned char spdy[] = {
    6,   's', 'p', 'd', 'y', '/', '4', 8,   's', 'p', 'd', 'y', '/',
    '2', '.', '1', 8,   'h', 't', 't', 'p', '/', '1', '.', '0',
  };
  unsigned char outlen = 0;
  const unsigned char *out = NULL;
  assert_int(-1, ==,
             nghttp2_select_next_protocol((unsigned char **)&out, &outlen, spdy,
                                          sizeof(spdy)));
  assert_uchar(0, ==, outlen);
  assert_null(out);

  outlen = 0;
  out = NULL;

  assert_int(-1, ==, nghttp2_select_alpn(&out, &outlen, spdy, sizeof(spdy)));
  assert_uchar(0, ==, outlen);
  assert_null(out);
}

void test_nghttp2_alpn(void) {
  http2();
  http11();
  no_overlap();
}
