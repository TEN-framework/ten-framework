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
#include "nghttp2_test_helper.h"

#include <stdio.h>
#include <assert.h>

#include "nghttp2_helper.h"
#include "nghttp2_priority_spec.h"

int unpack_framebuf(nghttp2_frame *frame, nghttp2_bufs *bufs) {
  nghttp2_buf *buf;

  /* Assuming we have required data in first buffer. We don't decode
     header block so, we don't mind its space */
  buf = &bufs->head->buf;
  return unpack_frame(frame, buf->pos, nghttp2_buf_len(buf));
}

int unpack_frame(nghttp2_frame *frame, const uint8_t *in, size_t len) {
  int rv = 0;
  const uint8_t *payload = in + NGHTTP2_FRAME_HDLEN;
  size_t payloadlen = len - NGHTTP2_FRAME_HDLEN;
  size_t payloadoff;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  nghttp2_frame_unpack_frame_hd(&frame->hd, in);
  switch (frame->hd.type) {
  case NGHTTP2_HEADERS:
    payloadoff = ((frame->hd.flags & NGHTTP2_FLAG_PADDED) > 0);
    nghttp2_frame_unpack_headers_payload(&frame->headers, payload + payloadoff);
    break;
  case NGHTTP2_PRIORITY:
    nghttp2_frame_unpack_priority_payload(&frame->priority, payload);
    break;
  case NGHTTP2_RST_STREAM:
    nghttp2_frame_unpack_rst_stream_payload(&frame->rst_stream, payload);
    break;
  case NGHTTP2_SETTINGS:
    rv = nghttp2_frame_unpack_settings_payload2(
      &frame->settings.iv, &frame->settings.niv, payload, payloadlen, mem);
    break;
  case NGHTTP2_PUSH_PROMISE:
    nghttp2_frame_unpack_push_promise_payload(&frame->push_promise, payload);
    break;
  case NGHTTP2_PING:
    nghttp2_frame_unpack_ping_payload(&frame->ping, payload);
    break;
  case NGHTTP2_GOAWAY:
    nghttp2_frame_unpack_goaway_payload2(&frame->goaway, payload, payloadlen,
                                         mem);
    break;
  case NGHTTP2_WINDOW_UPDATE:
    nghttp2_frame_unpack_window_update_payload(&frame->window_update, payload);
    break;
  case NGHTTP2_ALTSVC:
    assert(payloadlen > 2);
    nghttp2_frame_unpack_altsvc_payload2(&frame->ext, payload, payloadlen, mem);
    break;
  case NGHTTP2_ORIGIN:
    rv = nghttp2_frame_unpack_origin_payload(&frame->ext, payload, payloadlen,
                                             mem);
    break;
  case NGHTTP2_PRIORITY_UPDATE:
    assert(payloadlen >= 4);
    nghttp2_frame_unpack_priority_update_payload(
      &frame->ext, (uint8_t *)payload, payloadlen);
    break;
  default:
    /* Must not be reachable */
    assert(0);
  }
  return rv;
}

int strmemeq(const char *a, const uint8_t *b, size_t bn) {
  const uint8_t *c;
  if (!a || !b) {
    return 0;
  }
  c = b + bn;
  for (; *a && b != c && *a == *b; ++a, ++b)
    ;
  return !*a && b == c;
}

int nvnameeq(const char *a, nghttp2_nv *nv) {
  return strmemeq(a, nv->name, nv->namelen);
}

int nvvalueeq(const char *a, nghttp2_nv *nv) {
  return strmemeq(a, nv->value, nv->valuelen);
}

void nva_out_init(nva_out *out) {
  memset(out->nva, 0, sizeof(out->nva));
  out->nvlen = 0;
}

void nva_out_reset(nva_out *out, nghttp2_mem *mem) {
  size_t i;
  for (i = 0; i < out->nvlen; ++i) {
    mem->free(out->nva[i].name, NULL);
    mem->free(out->nva[i].value, NULL);
  }
  memset(out->nva, 0, sizeof(out->nva));
  out->nvlen = 0;
}

void add_out(nva_out *out, nghttp2_nv *nv, nghttp2_mem *mem) {
  nghttp2_nv *onv = &out->nva[out->nvlen];
  if (nv->namelen) {
    onv->name = mem->malloc(nv->namelen, NULL);
    memcpy(onv->name, nv->name, nv->namelen);
  } else {
    onv->name = NULL;
  }
  if (nv->valuelen) {
    onv->value = mem->malloc(nv->valuelen, NULL);
    memcpy(onv->value, nv->value, nv->valuelen);
  } else {
    onv->value = NULL;
  }
  onv->namelen = nv->namelen;
  onv->valuelen = nv->valuelen;

  onv->flags = nv->flags;

  ++out->nvlen;
}

nghttp2_ssize inflate_hd(nghttp2_hd_inflater *inflater, nva_out *out,
                         nghttp2_bufs *bufs, size_t offset, nghttp2_mem *mem) {
  nghttp2_ssize rv;
  nghttp2_nv nv;
  int inflate_flags;
  nghttp2_buf_chain *ci;
  nghttp2_buf *buf;
  nghttp2_buf bp;
  int fin;
  size_t processed;

  processed = 0;

  for (ci = bufs->head; ci; ci = ci->next) {
    buf = &ci->buf;
    fin = nghttp2_buf_len(buf) == 0 || ci->next == NULL;
    bp = *buf;

    if (offset) {
      size_t n;

      n = nghttp2_min_size(offset, nghttp2_buf_len(&bp));
      bp.pos += n;
      offset -= n;
    }

    for (;;) {
      inflate_flags = 0;
      rv = nghttp2_hd_inflate_hd3(inflater, &nv, &inflate_flags, bp.pos,
                                  nghttp2_buf_len(&bp), fin);

      if (rv < 0) {
        return rv;
      }

      bp.pos += rv;
      processed += (size_t)rv;

      if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
        if (out) {
          add_out(out, &nv, mem);
        }
      }
      if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
        break;
      }
      if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 &&
          nghttp2_buf_len(&bp) == 0) {
        break;
      }
    }
  }

  nghttp2_hd_inflate_end_headers(inflater);

  return (nghttp2_ssize)processed;
}

int pack_headers(nghttp2_bufs *bufs, nghttp2_hd_deflater *deflater,
                 int32_t stream_id, uint8_t flags, const nghttp2_nv *nva,
                 size_t nvlen, nghttp2_mem *mem) {
  nghttp2_nv *dnva;
  nghttp2_frame frame;
  int rv;

  nghttp2_nv_array_copy(&dnva, nva, nvlen, mem);

  nghttp2_frame_headers_init(&frame.headers, flags, stream_id,
                             NGHTTP2_HCAT_HEADERS, NULL, dnva, nvlen);
  rv = nghttp2_frame_pack_headers(bufs, &frame.headers, deflater);

  nghttp2_frame_headers_free(&frame.headers, mem);

  return rv;
}

int pack_push_promise(nghttp2_bufs *bufs, nghttp2_hd_deflater *deflater,
                      int32_t stream_id, uint8_t flags,
                      int32_t promised_stream_id, const nghttp2_nv *nva,
                      size_t nvlen, nghttp2_mem *mem) {
  nghttp2_nv *dnva;
  nghttp2_frame frame;
  int rv;

  nghttp2_nv_array_copy(&dnva, nva, nvlen, mem);

  nghttp2_frame_push_promise_init(&frame.push_promise, flags, stream_id,
                                  promised_stream_id, dnva, nvlen);
  rv = nghttp2_frame_pack_push_promise(bufs, &frame.push_promise, deflater);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);

  return rv;
}

int frame_pack_bufs_init(nghttp2_bufs *bufs) {
  /* 1 for Pad Length */
  return nghttp2_bufs_init2(bufs, 4096, 16, NGHTTP2_FRAME_HDLEN + 1,
                            nghttp2_mem_default());
}

void bufs_large_init(nghttp2_bufs *bufs, size_t chunk_size) {
  /* 1 for Pad Length */
  nghttp2_bufs_init2(bufs, chunk_size, 16, NGHTTP2_FRAME_HDLEN + 1,
                     nghttp2_mem_default());
}

nghttp2_stream *open_stream(nghttp2_session *session, int32_t stream_id) {
  return nghttp2_session_open_stream(
    session, stream_id, NGHTTP2_STREAM_FLAG_NONE, NGHTTP2_STREAM_OPENED, NULL);
}

nghttp2_outbound_item *create_data_ob_item(nghttp2_mem *mem) {
  nghttp2_outbound_item *item;

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);
  memset(item, 0, sizeof(nghttp2_outbound_item));

  return item;
}

nghttp2_stream *open_sent_stream(nghttp2_session *session, int32_t stream_id) {
  return open_sent_stream3(session, stream_id, NGHTTP2_FLAG_NONE,
                           NGHTTP2_STREAM_OPENED, NULL);
}

nghttp2_stream *open_sent_stream2(nghttp2_session *session, int32_t stream_id,
                                  nghttp2_stream_state initial_state) {
  return open_sent_stream3(session, stream_id, NGHTTP2_FLAG_NONE, initial_state,
                           NULL);
}

nghttp2_stream *open_sent_stream3(nghttp2_session *session, int32_t stream_id,
                                  uint8_t flags,
                                  nghttp2_stream_state initial_state,
                                  void *stream_user_data) {
  nghttp2_stream *stream;

  assert(nghttp2_session_is_my_stream_id(session, stream_id));

  stream = nghttp2_session_open_stream(session, stream_id, flags, initial_state,
                                       stream_user_data);
  session->last_sent_stream_id =
    nghttp2_max_int32(session->last_sent_stream_id, stream_id);
  session->next_stream_id =
    nghttp2_max_uint32(session->next_stream_id, (uint32_t)stream_id + 2);

  return stream;
}

nghttp2_stream *open_recv_stream(nghttp2_session *session, int32_t stream_id) {
  return open_recv_stream3(session, stream_id, NGHTTP2_FLAG_NONE,
                           NGHTTP2_STREAM_OPENED, NULL);
}

nghttp2_stream *open_recv_stream2(nghttp2_session *session, int32_t stream_id,
                                  nghttp2_stream_state initial_state) {
  return open_recv_stream3(session, stream_id, NGHTTP2_FLAG_NONE, initial_state,
                           NULL);
}

nghttp2_stream *open_recv_stream3(nghttp2_session *session, int32_t stream_id,
                                  uint8_t flags,
                                  nghttp2_stream_state initial_state,
                                  void *stream_user_data) {
  nghttp2_stream *stream;

  assert(!nghttp2_session_is_my_stream_id(session, stream_id));

  stream = nghttp2_session_open_stream(session, stream_id, flags, initial_state,
                                       stream_user_data);
  session->last_recv_stream_id =
    nghttp2_max_int32(session->last_recv_stream_id, stream_id);

  return stream;
}
