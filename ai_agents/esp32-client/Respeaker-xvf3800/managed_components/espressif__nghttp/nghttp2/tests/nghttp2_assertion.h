/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2024 nghttp2 contributors
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
#ifndef NGHTTP2_ASSERTION_H
#define NGHTTP2_ASSERTION_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include "munit.h"

#include "nghttp2_frame.h"

#define assert_nv_equal(A, B, len, mem)                                        \
  do {                                                                         \
    size_t alloclen = sizeof(nghttp2_nv) * (len);                              \
    const nghttp2_nv *sa = (A), *sb = (B);                                     \
    nghttp2_nv *a = (mem)->malloc(alloclen, NULL);                             \
    nghttp2_nv *b = (mem)->malloc(alloclen, NULL);                             \
    size_t i_;                                                                 \
    memcpy(a, sa, alloclen);                                                   \
    memcpy(b, sb, alloclen);                                                   \
    nghttp2_nv_array_sort(a, (len));                                           \
    nghttp2_nv_array_sort(b, (len));                                           \
    for (i_ = 0; i_ < (size_t)(len); ++i_) {                                   \
      assert_memn_equal(a[i_].name, a[i_].namelen, b[i_].name, b[i_].namelen); \
      assert_memn_equal(a[i_].value, a[i_].valuelen, b[i_].value,              \
                        b[i_].valuelen);                                       \
    }                                                                          \
    (mem)->free(b, NULL);                                                      \
    (mem)->free(a, NULL);                                                      \
  } while (0);

#endif /* NGHTTP2_ASSERTION_H */
