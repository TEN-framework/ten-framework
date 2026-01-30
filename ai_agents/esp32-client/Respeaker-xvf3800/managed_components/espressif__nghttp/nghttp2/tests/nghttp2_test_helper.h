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
#ifndef NGHTTP2_TEST_HELPER_H
#define NGHTTP2_TEST_HELPER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include "nghttp2_frame.h"
#include "nghttp2_hd.h"
#include "nghttp2_session.h"

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)(NAME),   (uint8_t *)(VALUE),   sizeof((NAME)) - 1,             \
    sizeof((VALUE)) - 1, NGHTTP2_NV_FLAG_NONE,                                 \
  }
#define ARRLEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

int unpack_framebuf(nghttp2_frame *frame, nghttp2_bufs *bufs);

int unpack_frame(nghttp2_frame *frame, const uint8_t *in, size_t len);

int strmemeq(const char *a, const uint8_t *b, size_t bn);

int nvnameeq(const char *a, nghttp2_nv *nv);

int nvvalueeq(const char *a, nghttp2_nv *nv);

typedef struct {
  nghttp2_nv nva[256];
  size_t nvlen;
} nva_out;

void nva_out_init(nva_out *out);
void nva_out_reset(nva_out *out, nghttp2_mem *mem);

void add_out(nva_out *out, nghttp2_nv *nv, nghttp2_mem *mem);

nghttp2_ssize inflate_hd(nghttp2_hd_inflater *inflater, nva_out *out,
                         nghttp2_bufs *bufs, size_t offset, nghttp2_mem *mem);

int pack_headers(nghttp2_bufs *bufs, nghttp2_hd_deflater *deflater,
                 int32_t stream_id, uint8_t flags, const nghttp2_nv *nva,
                 size_t nvlen, nghttp2_mem *mem);

int pack_push_promise(nghttp2_bufs *bufs, nghttp2_hd_deflater *deflater,
                      int32_t stream_id, uint8_t flags,
                      int32_t promised_stream_id, const nghttp2_nv *nva,
                      size_t nvlen, nghttp2_mem *mem);

int frame_pack_bufs_init(nghttp2_bufs *bufs);

void bufs_large_init(nghttp2_bufs *bufs, size_t chunk_size);

nghttp2_stream *open_stream(nghttp2_session *session, int32_t stream_id);

nghttp2_outbound_item *create_data_ob_item(nghttp2_mem *mem);

/* Opens stream.  This stream is assumed to be sent from |session|,
   and session->last_sent_stream_id and session->next_stream_id will
   be adjusted accordingly. */
nghttp2_stream *open_sent_stream(nghttp2_session *session, int32_t stream_id);

nghttp2_stream *open_sent_stream2(nghttp2_session *session, int32_t stream_id,
                                  nghttp2_stream_state initial_state);

nghttp2_stream *open_sent_stream3(nghttp2_session *session, int32_t stream_id,
                                  uint8_t flags,
                                  nghttp2_stream_state initial_state,
                                  void *stream_user_data);

/* Opens stream.  This stream is assumed to be received by |session|,
   and session->last_recv_stream_id will be adjusted accordingly. */
nghttp2_stream *open_recv_stream(nghttp2_session *session, int32_t stream_id);

nghttp2_stream *open_recv_stream2(nghttp2_session *session, int32_t stream_id,
                                  nghttp2_stream_state initial_state);

nghttp2_stream *open_recv_stream3(nghttp2_session *session, int32_t stream_id,
                                  uint8_t flags,
                                  nghttp2_stream_state initial_state,
                                  void *stream_user_data);

#endif /* NGHTTP2_TEST_HELPER_H */
