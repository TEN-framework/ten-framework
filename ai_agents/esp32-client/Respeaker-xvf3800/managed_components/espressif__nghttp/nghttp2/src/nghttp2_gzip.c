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
#include "nghttp2_gzip.h"

#include <assert.h>

int nghttp2_gzip_inflate_new(nghttp2_gzip **inflater_ptr) {
  int rv;
  *inflater_ptr = calloc(1, sizeof(nghttp2_gzip));
  if (*inflater_ptr == NULL) {
    return -1;
  }
  rv = inflateInit2(&(*inflater_ptr)->zst, 47);
  if (rv != Z_OK) {
    free(*inflater_ptr);
    return -1;
  }
  return 0;
}

void nghttp2_gzip_inflate_del(nghttp2_gzip *inflater) {
  if (inflater != NULL) {
    inflateEnd(&inflater->zst);
    free(inflater);
  }
}

int nghttp2_gzip_inflate(nghttp2_gzip *inflater, uint8_t *out,
                         size_t *outlen_ptr, const uint8_t *in,
                         size_t *inlen_ptr) {
  int rv;
  if (inflater->finished) {
    return -1;
  }
  inflater->zst.avail_in = (unsigned int)*inlen_ptr;
  inflater->zst.next_in = (unsigned char *)in;
  inflater->zst.avail_out = (unsigned int)*outlen_ptr;
  inflater->zst.next_out = out;

  rv = inflate(&inflater->zst, Z_NO_FLUSH);

  *inlen_ptr -= inflater->zst.avail_in;
  *outlen_ptr -= inflater->zst.avail_out;
  switch (rv) {
  case Z_STREAM_END:
    inflater->finished = 1;
  /* FALL THROUGH */
  case Z_OK:
  case Z_BUF_ERROR:
    return 0;
  case Z_DATA_ERROR:
  case Z_STREAM_ERROR:
  case Z_NEED_DICT:
  case Z_MEM_ERROR:
    return -1;
  default:
    assert(0);
    /* We need this for some compilers */
    return 0;
  }
}

int nghttp2_gzip_inflate_finished(nghttp2_gzip *inflater) {
  return inflater->finished;
}
