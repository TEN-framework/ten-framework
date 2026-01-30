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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* !HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#define MAKE_NV(K, V)                                                          \
  {                                                                            \
    (uint8_t *)K,  (uint8_t *)V,         sizeof(K) - 1,                        \
    sizeof(V) - 1, NGHTTP2_NV_FLAG_NONE,                                       \
  }

static void deflate(nghttp2_hd_deflater *deflater,
                    nghttp2_hd_inflater *inflater, const nghttp2_nv *const nva,
                    size_t nvlen);

static int inflate_header_block(nghttp2_hd_inflater *inflater, uint8_t *in,
                                size_t inlen, int final);

int main(void) {
  int rv;
  nghttp2_hd_deflater *deflater;
  nghttp2_hd_inflater *inflater;
  /* Define 1st header set.  This is looks like a HTTP request. */
  nghttp2_nv nva1[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "example.org"),
    MAKE_NV(":path", "/"), MAKE_NV("user-agent", "libnghttp2"),
    MAKE_NV("accept-encoding", "gzip, deflate")};
  /* Define 2nd header set */
  nghttp2_nv nva2[] = {MAKE_NV(":scheme", "https"),
                       MAKE_NV(":authority", "example.org"),
                       MAKE_NV(":path", "/stylesheet/style.css"),
                       MAKE_NV("user-agent", "libnghttp2"),
                       MAKE_NV("accept-encoding", "gzip, deflate"),
                       MAKE_NV("referer", "https://example.org")};

  rv = nghttp2_hd_deflate_new(&deflater, 4096);

  if (rv != 0) {
    fprintf(stderr, "nghttp2_hd_deflate_init failed with error: %s\n",
            nghttp2_strerror(rv));
    exit(EXIT_FAILURE);
  }

  rv = nghttp2_hd_inflate_new(&inflater);

  if (rv != 0) {
    fprintf(stderr, "nghttp2_hd_inflate_init failed with error: %s\n",
            nghttp2_strerror(rv));
    exit(EXIT_FAILURE);
  }

  /* Encode and decode 1st header set */
  deflate(deflater, inflater, nva1, sizeof(nva1) / sizeof(nva1[0]));

  /* Encode and decode 2nd header set, using differential encoding
     using state after encoding 1st header set. */
  deflate(deflater, inflater, nva2, sizeof(nva2) / sizeof(nva2[0]));

  nghttp2_hd_inflate_del(inflater);
  nghttp2_hd_deflate_del(deflater);

  return 0;
}

static void deflate(nghttp2_hd_deflater *deflater,
                    nghttp2_hd_inflater *inflater, const nghttp2_nv *const nva,
                    size_t nvlen) {
  nghttp2_ssize rv;
  uint8_t *buf;
  size_t buflen;
  size_t outlen;
  size_t i;
  size_t sum;

  sum = 0;

  for (i = 0; i < nvlen; ++i) {
    sum += nva[i].namelen + nva[i].valuelen;
  }

  printf("Input (%zu byte(s)):\n\n", sum);

  for (i = 0; i < nvlen; ++i) {
    fwrite(nva[i].name, 1, nva[i].namelen, stdout);
    printf(": ");
    fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
    printf("\n");
  }

  buflen = nghttp2_hd_deflate_bound(deflater, nva, nvlen);
  buf = malloc(buflen);

  rv = nghttp2_hd_deflate_hd2(deflater, buf, buflen, nva, nvlen);

  if (rv < 0) {
    fprintf(stderr, "nghttp2_hd_deflate_hd2() failed with error: %s\n",
            nghttp2_strerror((int)rv));

    free(buf);

    exit(EXIT_FAILURE);
  }

  outlen = (size_t)rv;

  printf("\nDeflate (%zu byte(s), ratio %.02f):\n\n", outlen,
         sum == 0 ? 0 : (double)outlen / (double)sum);

  for (i = 0; i < outlen; ++i) {
    if ((i & 0x0fu) == 0) {
      printf("%08zX: ", i);
    }

    printf("%02X ", buf[i]);

    if (((i + 1) & 0x0fu) == 0) {
      printf("\n");
    }
  }

  printf("\n\nInflate:\n\n");

  /* We pass 1 to final parameter, because buf contains whole deflated
     header data. */
  rv = inflate_header_block(inflater, buf, outlen, 1);

  if (rv != 0) {
    free(buf);

    exit(EXIT_FAILURE);
  }

  printf("\n-----------------------------------------------------------"
         "--------------------\n");

  free(buf);
}

int inflate_header_block(nghttp2_hd_inflater *inflater, uint8_t *in,
                         size_t inlen, int final) {
  nghttp2_ssize rv;

  for (;;) {
    nghttp2_nv nv;
    int inflate_flags = 0;
    size_t proclen;

    rv =
      nghttp2_hd_inflate_hd3(inflater, &nv, &inflate_flags, in, inlen, final);

    if (rv < 0) {
      fprintf(stderr, "inflate failed with error code %td", rv);
      return -1;
    }

    proclen = (size_t)rv;

    in += proclen;
    inlen -= proclen;

    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      fwrite(nv.name, 1, nv.namelen, stderr);
      fprintf(stderr, ": ");
      fwrite(nv.value, 1, nv.valuelen, stderr);
      fprintf(stderr, "\n");
    }

    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
      nghttp2_hd_inflate_end_headers(inflater);
      break;
    }

    if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
      break;
    }
  }

  return 0;
}
