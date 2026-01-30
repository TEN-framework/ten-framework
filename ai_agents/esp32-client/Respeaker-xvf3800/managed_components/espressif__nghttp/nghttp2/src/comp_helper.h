/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#ifndef NGHTTP2_COMP_HELPER_H
#define NGHTTP2_COMP_HELPER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <jansson.h>

#include <nghttp2/nghttp2.h>

#ifdef __cplusplus
extern "C" {
#endif

json_t *dump_deflate_header_table(nghttp2_hd_deflater *deflater);

json_t *dump_inflate_header_table(nghttp2_hd_inflater *inflater);

json_t *dump_header(const uint8_t *name, size_t namelen, const uint8_t *value,
                    size_t vlauelen);

json_t *dump_headers(const nghttp2_nv *nva, size_t nvlen);

void output_json_header(void);

void output_json_footer(void);

#ifdef __cplusplus
}
#endif

#endif /* NGHTTP2_COMP_HELPER_H */
