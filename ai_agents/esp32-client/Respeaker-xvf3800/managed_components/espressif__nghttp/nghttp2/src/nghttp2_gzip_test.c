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
#include "nghttp2_gzip_test.h"

#include <stdio.h>
#include <assert.h>

#include "munit.h"

#include <zlib.h>

#include "nghttp2_gzip.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_gzip_inflate),
  munit_test_end(),
};

const MunitSuite gzip_suite = {
  "/gzip", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

static size_t deflate_data(uint8_t *out, size_t outlen, const uint8_t *in,
                           size_t inlen) {
  int rv;
  z_stream zst = {0};

  rv = deflateInit(&zst, Z_DEFAULT_COMPRESSION);
  assert_int(Z_OK, ==, rv);

  zst.avail_in = (unsigned int)inlen;
  zst.next_in = (uint8_t *)in;
  zst.avail_out = (unsigned int)outlen;
  zst.next_out = out;
  rv = deflate(&zst, Z_SYNC_FLUSH);
  assert_int(Z_OK, ==, rv);

  deflateEnd(&zst);

  return outlen - zst.avail_out;
}

static const char input[] =
  "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND "
  "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
  "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND "
  "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE "
  "LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION "
  "OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION "
  "WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";

void test_nghttp2_gzip_inflate(void) {
  nghttp2_gzip *inflater;
  uint8_t in[4096], out[4096], *inptr;
  size_t inlen = sizeof(in);
  size_t inproclen, outproclen;
  const char *inputptr = input;

  inlen = deflate_data(in, inlen, (const uint8_t *)input, sizeof(input) - 1);

  assert_int(0, ==, nghttp2_gzip_inflate_new(&inflater));
  /* First 16 bytes */
  inptr = in;
  inproclen = inlen;
  outproclen = 16;
  assert_int(
    0, ==, nghttp2_gzip_inflate(inflater, out, &outproclen, inptr, &inproclen));
  assert_size(16, ==, outproclen);
  assert_size(0, <, inproclen);
  assert_memory_equal(outproclen, inputptr, out);
  /* Next 32 bytes */
  inptr += inproclen;
  inlen -= inproclen;
  inproclen = inlen;
  inputptr += outproclen;
  outproclen = 32;
  assert_int(
    0, ==, nghttp2_gzip_inflate(inflater, out, &outproclen, inptr, &inproclen));
  assert_size(32, ==, outproclen);
  assert_size(0, <, inproclen);
  assert_memory_equal(outproclen, inputptr, out);
  /* Rest */
  inptr += inproclen;
  inlen -= inproclen;
  inproclen = inlen;
  inputptr += outproclen;
  outproclen = sizeof(out);
  assert_int(
    0, ==, nghttp2_gzip_inflate(inflater, out, &outproclen, inptr, &inproclen));
  assert_size(sizeof(input) - 49, ==, outproclen);
  assert_size(0, <, inproclen);
  assert_memory_equal(outproclen, inputptr, out);

  inlen -= inproclen;
  assert_size(0, ==, inlen);

  nghttp2_gzip_inflate_del(inflater);
}
