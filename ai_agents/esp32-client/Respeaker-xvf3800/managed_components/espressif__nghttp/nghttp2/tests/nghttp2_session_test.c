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
#include "nghttp2_session_test.h"

#include <stdio.h>
#include <assert.h>

#include "munit.h"

#include "nghttp2_session.h"
#include "nghttp2_stream.h"
#include "nghttp2_net.h"
#include "nghttp2_helper.h"
#include "nghttp2_test_helper.h"
#include "nghttp2_assertion.h"
#include "nghttp2_priority_spec.h"
#include "nghttp2_extpri.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_session_recv),
  munit_void_test(test_nghttp2_session_recv_invalid_stream_id),
  munit_void_test(test_nghttp2_session_recv_invalid_frame),
  munit_void_test(test_nghttp2_session_recv_eof),
  munit_void_test(test_nghttp2_session_recv_data),
  munit_void_test(test_nghttp2_session_recv_data_no_auto_flow_control),
  munit_void_test(test_nghttp2_session_recv_continuation),
  munit_void_test(test_nghttp2_session_recv_headers_with_priority),
  munit_void_test(test_nghttp2_session_recv_headers_with_padding),
  munit_void_test(test_nghttp2_session_recv_headers_early_response),
  munit_void_test(test_nghttp2_session_recv_headers_for_closed_stream),
  munit_void_test(test_nghttp2_session_recv_headers_with_extpri),
  munit_void_test(test_nghttp2_session_server_recv_push_response),
  munit_void_test(test_nghttp2_session_recv_premature_headers),
  munit_void_test(test_nghttp2_session_recv_unknown_frame),
  munit_void_test(test_nghttp2_session_recv_unexpected_continuation),
  munit_void_test(test_nghttp2_session_recv_settings_header_table_size),
  munit_void_test(test_nghttp2_session_recv_too_large_frame_length),
  munit_void_test(test_nghttp2_session_recv_extension),
  munit_void_test(test_nghttp2_session_recv_altsvc),
  munit_void_test(test_nghttp2_session_recv_origin),
  munit_void_test(test_nghttp2_session_recv_priority_update),
  munit_void_test(test_nghttp2_session_continue),
  munit_void_test(test_nghttp2_session_add_frame),
  munit_void_test(test_nghttp2_session_on_request_headers_received),
  munit_void_test(test_nghttp2_session_on_response_headers_received),
  munit_void_test(test_nghttp2_session_on_headers_received),
  munit_void_test(test_nghttp2_session_on_push_response_headers_received),
  munit_void_test(test_nghttp2_session_on_rst_stream_received),
  munit_void_test(test_nghttp2_session_on_settings_received),
  munit_void_test(test_nghttp2_session_on_push_promise_received),
  munit_void_test(test_nghttp2_session_on_ping_received),
  munit_void_test(test_nghttp2_session_on_goaway_received),
  munit_void_test(test_nghttp2_session_on_window_update_received),
  munit_void_test(test_nghttp2_session_on_data_received),
  munit_void_test(test_nghttp2_session_on_data_received_fail_fast),
  munit_void_test(test_nghttp2_session_on_altsvc_received),
  munit_void_test(test_nghttp2_session_send_headers_start_stream),
  munit_void_test(test_nghttp2_session_send_headers_reply),
  munit_void_test(test_nghttp2_session_send_headers_frame_size_error),
  munit_void_test(test_nghttp2_session_send_headers_push_reply),
  munit_void_test(test_nghttp2_session_send_rst_stream),
  munit_void_test(test_nghttp2_session_send_push_promise),
  munit_void_test(test_nghttp2_session_is_my_stream_id),
  munit_void_test(test_nghttp2_session_upgrade2),
  munit_void_test(test_nghttp2_submit_data),
  munit_void_test(test_nghttp2_submit_data_read_length_too_large),
  munit_void_test(test_nghttp2_submit_data_read_length_smallest),
  munit_void_test(test_nghttp2_submit_data_twice),
  munit_void_test(test_nghttp2_submit_request_with_data),
  munit_void_test(test_nghttp2_submit_request_without_data),
  munit_void_test(test_nghttp2_submit_response_with_data),
  munit_void_test(test_nghttp2_submit_response_without_data),
  munit_void_test(test_nghttp2_submit_response_push_response),
  munit_void_test(test_nghttp2_submit_trailer),
  munit_void_test(test_nghttp2_submit_headers_start_stream),
  munit_void_test(test_nghttp2_submit_headers_reply),
  munit_void_test(test_nghttp2_submit_headers_push_reply),
  munit_void_test(test_nghttp2_submit_headers),
  munit_void_test(test_nghttp2_submit_headers_continuation),
  munit_void_test(test_nghttp2_submit_headers_continuation_extra_large),
  munit_void_test(test_nghttp2_submit_settings),
  munit_void_test(test_nghttp2_submit_settings_update_local_window_size),
  munit_void_test(test_nghttp2_submit_settings_multiple_times),
  munit_void_test(test_nghttp2_submit_push_promise),
  munit_void_test(test_nghttp2_submit_window_update),
  munit_void_test(test_nghttp2_submit_window_update_local_window_size),
  munit_void_test(test_nghttp2_submit_shutdown_notice),
  munit_void_test(test_nghttp2_submit_invalid_nv),
  munit_void_test(test_nghttp2_submit_extension),
  munit_void_test(test_nghttp2_submit_altsvc),
  munit_void_test(test_nghttp2_submit_origin),
  munit_void_test(test_nghttp2_submit_priority_update),
  munit_void_test(test_nghttp2_submit_rst_stream),
  munit_void_test(test_nghttp2_session_open_stream),
  munit_void_test(test_nghttp2_session_get_next_ob_item),
  munit_void_test(test_nghttp2_session_pop_next_ob_item),
  munit_void_test(test_nghttp2_session_reply_fail),
  munit_void_test(test_nghttp2_session_max_concurrent_streams),
  munit_void_test(test_nghttp2_session_stop_data_with_rst_stream),
  munit_void_test(test_nghttp2_session_defer_data),
  munit_void_test(test_nghttp2_session_flow_control),
  munit_void_test(test_nghttp2_session_flow_control_data_recv),
  munit_void_test(test_nghttp2_session_flow_control_data_with_padding_recv),
  munit_void_test(test_nghttp2_session_data_read_temporal_failure),
  munit_void_test(test_nghttp2_session_on_stream_close),
  munit_void_test(test_nghttp2_session_on_ctrl_not_send),
  munit_void_test(test_nghttp2_session_get_outbound_queue_size),
  munit_void_test(test_nghttp2_session_get_effective_local_window_size),
  munit_void_test(test_nghttp2_session_set_option),
  munit_void_test(test_nghttp2_session_data_backoff_by_high_pri_frame),
  munit_void_test(test_nghttp2_session_pack_data_with_padding),
  munit_void_test(test_nghttp2_session_pack_headers_with_padding),
  munit_void_test(test_nghttp2_pack_settings_payload),
  munit_void_test(test_nghttp2_session_stream_get_state),
  munit_void_test(test_nghttp2_session_find_stream),
  munit_void_test(test_nghttp2_session_graceful_shutdown),
  munit_void_test(test_nghttp2_session_on_header_temporal_failure),
  munit_void_test(test_nghttp2_session_recv_client_magic),
  munit_void_test(test_nghttp2_session_delete_data_item),
  munit_void_test(test_nghttp2_session_open_idle_stream),
  munit_void_test(test_nghttp2_session_cancel_reserved_remote),
  munit_void_test(test_nghttp2_session_reset_pending_headers),
  munit_void_test(test_nghttp2_session_send_data_callback),
  munit_void_test(test_nghttp2_session_on_begin_headers_temporal_failure),
  munit_void_test(test_nghttp2_session_defer_then_close),
  munit_void_test(test_nghttp2_session_detach_item_from_closed_stream),
  munit_void_test(test_nghttp2_session_flooding),
  munit_void_test(test_nghttp2_session_change_extpri_stream_priority),
  munit_void_test(test_nghttp2_session_set_local_window_size),
  munit_void_test(test_nghttp2_session_cancel_from_before_frame_send),
  munit_void_test(test_nghttp2_session_too_many_settings),
  munit_void_test(test_nghttp2_session_removed_closed_stream),
  munit_void_test(test_nghttp2_session_pause_data),
  munit_void_test(test_nghttp2_session_no_closed_streams),
  munit_void_test(test_nghttp2_session_set_stream_user_data),
  munit_void_test(test_nghttp2_session_no_rfc7540_priorities),
  munit_void_test(test_nghttp2_session_stream_reset_ratelim),
  munit_void_test(test_nghttp2_http_mandatory_headers),
  munit_void_test(test_nghttp2_http_content_length),
  munit_void_test(test_nghttp2_http_content_length_mismatch),
  munit_void_test(test_nghttp2_http_non_final_response),
  munit_void_test(test_nghttp2_http_trailer_headers),
  munit_void_test(test_nghttp2_http_ignore_regular_header),
  munit_void_test(test_nghttp2_http_ignore_content_length),
  munit_void_test(test_nghttp2_http_record_request_method),
  munit_void_test(test_nghttp2_http_push_promise),
  munit_void_test(test_nghttp2_http_head_method_upgrade_workaround),
  munit_void_test(
    test_nghttp2_http_no_rfc9113_leading_and_trailing_ws_validation),
  munit_test_end(),
};

const MunitSuite session_suite = {
  "/session", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

typedef struct {
  uint8_t buf[65535];
  size_t length;
} accumulator;

typedef struct {
  uint8_t data[8192];
  uint8_t *datamark;
  uint8_t *datalimit;
  size_t feedseq[8192];
  size_t seqidx;
} scripted_data_feed;

typedef struct {
  accumulator *acc;
  scripted_data_feed *df;
  int frame_recv_cb_called, invalid_frame_recv_cb_called;
  uint8_t recv_frame_type;
  nghttp2_frame_hd recv_frame_hd;
  int frame_send_cb_called;
  uint8_t sent_frame_type;
  int before_frame_send_cb_called;
  int frame_not_send_cb_called;
  uint8_t not_sent_frame_type;
  int not_sent_error;
  int stream_close_cb_called;
  uint32_t stream_close_error_code;
  size_t data_source_length;
  int32_t stream_id;
  size_t block_count;
  int data_chunk_recv_cb_called;
  const nghttp2_frame *frame;
  size_t fixed_sendlen;
  int header_cb_called;
  int invalid_header_cb_called;
  int begin_headers_cb_called;
  nghttp2_nv nv;
  size_t data_chunk_len;
  size_t padlen;
  int begin_frame_cb_called;
  nghttp2_buf scratchbuf;
  size_t data_source_read_cb_paused;
} my_user_data;

static const nghttp2_nv reqnv[] = {
  MAKE_NV(":method", "GET"),
  MAKE_NV(":path", "/"),
  MAKE_NV(":scheme", "https"),
  MAKE_NV(":authority", "localhost"),
};

static const nghttp2_nv resnv[] = {
  MAKE_NV(":status", "200"),
};

static const nghttp2_nv trailernv[] = {
  // from http://tools.ietf.org/html/rfc6249#section-7
  MAKE_NV("digest", "SHA-256="
                    "MWVkMWQxYTRiMzk5MDQ0MzI3NGU5NDEyZTk5OWY1ZGFmNzgyZTJlODYz"
                    "YjRjYzFhOTlmNTQwYzI2M2QwM2U2MQ=="),
};

static void scripted_data_feed_init2(scripted_data_feed *df,
                                     nghttp2_bufs *bufs) {
  nghttp2_buf_chain *ci;
  nghttp2_buf *buf;
  uint8_t *ptr;
  size_t len;

  memset(df, 0, sizeof(scripted_data_feed));
  ptr = df->data;
  len = 0;

  for (ci = bufs->head; ci; ci = ci->next) {
    buf = &ci->buf;
    ptr = nghttp2_cpymem(ptr, buf->pos, nghttp2_buf_len(buf));
    len += nghttp2_buf_len(buf);
  }

  df->datamark = df->data;
  df->datalimit = df->data + len;
  df->feedseq[0] = len;
}

static nghttp2_ssize null_send_callback(nghttp2_session *session,
                                        const uint8_t *data, size_t len,
                                        int flags, void *user_data) {
  (void)session;
  (void)data;
  (void)flags;
  (void)user_data;

  return (nghttp2_ssize)len;
}

static nghttp2_ssize fail_send_callback(nghttp2_session *session,
                                        const uint8_t *data, size_t len,
                                        int flags, void *user_data) {
  (void)session;
  (void)data;
  (void)len;
  (void)flags;
  (void)user_data;

  return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static nghttp2_ssize fixed_bytes_send_callback(nghttp2_session *session,
                                               const uint8_t *data, size_t len,
                                               int flags, void *user_data) {
  size_t fixed_sendlen = ((my_user_data *)user_data)->fixed_sendlen;
  (void)session;
  (void)data;
  (void)flags;

  return (nghttp2_ssize)(fixed_sendlen < len ? fixed_sendlen : len);
}

static nghttp2_ssize scripted_recv_callback(nghttp2_session *session,
                                            uint8_t *data, size_t len,
                                            int flags, void *user_data) {
  scripted_data_feed *df = ((my_user_data *)user_data)->df;
  size_t wlen = df->feedseq[df->seqidx] > len ? len : df->feedseq[df->seqidx];
  (void)session;
  (void)flags;

  memcpy(data, df->datamark, wlen);
  df->datamark += wlen;
  df->feedseq[df->seqidx] -= wlen;
  if (df->feedseq[df->seqidx] == 0) {
    ++df->seqidx;
  }
  return (nghttp2_ssize)wlen;
}

static nghttp2_ssize eof_recv_callback(nghttp2_session *session, uint8_t *data,
                                       size_t len, int flags, void *user_data) {
  (void)session;
  (void)data;
  (void)len;
  (void)flags;
  (void)user_data;

  return NGHTTP2_ERR_EOF;
}

static nghttp2_ssize accumulator_send_callback(nghttp2_session *session,
                                               const uint8_t *buf, size_t len,
                                               int flags, void *user_data) {
  accumulator *acc = ((my_user_data *)user_data)->acc;
  (void)session;
  (void)flags;

  assert(acc->length + len < sizeof(acc->buf));
  memcpy(acc->buf + acc->length, buf, len);
  acc->length += len;
  return (nghttp2_ssize)len;
}

static int on_begin_frame_callback(nghttp2_session *session,
                                   const nghttp2_frame_hd *hd,
                                   void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)hd;

  ++ud->begin_frame_cb_called;
  return 0;
}

static int on_frame_recv_callback(nghttp2_session *session,
                                  const nghttp2_frame *frame, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;

  ++ud->frame_recv_cb_called;
  ud->recv_frame_type = frame->hd.type;
  ud->recv_frame_hd = frame->hd;

  return 0;
}

static int on_invalid_frame_recv_callback(nghttp2_session *session,
                                          const nghttp2_frame *frame,
                                          int lib_error_code, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)frame;
  (void)lib_error_code;

  ++ud->invalid_frame_recv_cb_called;
  return 0;
}

static int on_frame_send_callback(nghttp2_session *session,
                                  const nghttp2_frame *frame, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;

  ++ud->frame_send_cb_called;
  ud->sent_frame_type = frame->hd.type;
  return 0;
}

static int on_frame_not_send_callback(nghttp2_session *session,
                                      const nghttp2_frame *frame, int lib_error,
                                      void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;

  ++ud->frame_not_send_cb_called;
  ud->not_sent_frame_type = frame->hd.type;
  ud->not_sent_error = lib_error;
  return 0;
}

static int cancel_before_frame_send_callback(nghttp2_session *session,
                                             const nghttp2_frame *frame,
                                             void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)frame;

  ++ud->before_frame_send_cb_called;
  return NGHTTP2_ERR_CANCEL;
}

static int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags,
                                       int32_t stream_id, const uint8_t *data,
                                       size_t len, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)flags;
  (void)stream_id;
  (void)data;

  ++ud->data_chunk_recv_cb_called;
  ud->data_chunk_len = len;
  return 0;
}

static int pause_on_data_chunk_recv_callback(nghttp2_session *session,
                                             uint8_t flags, int32_t stream_id,
                                             const uint8_t *data, size_t len,
                                             void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)flags;
  (void)stream_id;
  (void)data;
  (void)len;

  ++ud->data_chunk_recv_cb_called;
  return NGHTTP2_ERR_PAUSE;
}

static nghttp2_ssize select_padding_callback(nghttp2_session *session,
                                             const nghttp2_frame *frame,
                                             size_t max_payloadlen,
                                             void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;

  return (nghttp2_ssize)nghttp2_min_size(max_payloadlen,
                                         frame->hd.length + ud->padlen);
}

static nghttp2_ssize too_large_data_source_length_callback(
  nghttp2_session *session, uint8_t frame_type, int32_t stream_id,
  int32_t session_remote_window_size, int32_t stream_remote_window_size,
  uint32_t remote_max_frame_size, void *user_data) {
  (void)session;
  (void)frame_type;
  (void)stream_id;
  (void)session_remote_window_size;
  (void)stream_remote_window_size;
  (void)remote_max_frame_size;
  (void)user_data;

  return NGHTTP2_MAX_FRAME_SIZE_MAX + 1;
}

static nghttp2_ssize smallest_length_data_source_length_callback(
  nghttp2_session *session, uint8_t frame_type, int32_t stream_id,
  int32_t session_remote_window_size, int32_t stream_remote_window_size,
  uint32_t remote_max_frame_size, void *user_data) {
  (void)session;
  (void)frame_type;
  (void)stream_id;
  (void)session_remote_window_size;
  (void)stream_remote_window_size;
  (void)remote_max_frame_size;
  (void)user_data;

  return 1;
}

static nghttp2_ssize fixed_length_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  size_t wlen;
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)source;

  if (len < ud->data_source_length) {
    wlen = len;
  } else {
    wlen = ud->data_source_length;
  }
  ud->data_source_length -= wlen;
  if (ud->data_source_length == 0) {
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
  }
  return (nghttp2_ssize)wlen;
}

static nghttp2_ssize temporal_failure_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)len;
  (void)data_flags;
  (void)source;
  (void)user_data;

  return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
}

static nghttp2_ssize
fail_data_source_read_callback(nghttp2_session *session, int32_t stream_id,
                               uint8_t *buf, size_t len, uint32_t *data_flags,
                               nghttp2_data_source *source, void *user_data) {
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)len;
  (void)data_flags;
  (void)source;
  (void)user_data;

  return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static nghttp2_ssize no_end_stream_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)len;
  (void)source;
  (void)user_data;

  *data_flags |= NGHTTP2_DATA_FLAG_EOF | NGHTTP2_DATA_FLAG_NO_END_STREAM;
  return 0;
}

static nghttp2_ssize no_copy_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  size_t wlen;
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)source;

  if (len < ud->data_source_length) {
    wlen = len;
  } else {
    wlen = ud->data_source_length;
  }

  ud->data_source_length -= wlen;

  *data_flags |= NGHTTP2_DATA_FLAG_NO_COPY;

  if (ud->data_source_length == 0) {
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
  }
  return (nghttp2_ssize)wlen;
}

static int send_data_callback(nghttp2_session *session, nghttp2_frame *frame,
                              const uint8_t *framehd, size_t length,
                              nghttp2_data_source *source, void *user_data) {
  accumulator *acc = ((my_user_data *)user_data)->acc;
  (void)session;
  (void)source;

  memcpy(acc->buf + acc->length, framehd, NGHTTP2_FRAME_HDLEN);
  acc->length += NGHTTP2_FRAME_HDLEN;

  if (frame->data.padlen) {
    *(acc->buf + acc->length++) = (uint8_t)(frame->data.padlen - 1);
  }

  acc->length += length;

  if (frame->data.padlen) {
    acc->length += frame->data.padlen - 1;
  }

  return 0;
}

static nghttp2_ssize block_count_send_callback(nghttp2_session *session,
                                               const uint8_t *data, size_t len,
                                               int flags, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)data;
  (void)flags;

  if (ud->block_count == 0) {
    return NGHTTP2_ERR_WOULDBLOCK;
  }

  --ud->block_count;
  return (nghttp2_ssize)len;
}

static int on_header_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, const uint8_t *name,
                              size_t namelen, const uint8_t *value,
                              size_t valuelen, uint8_t flags, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)flags;

  ++ud->header_cb_called;
  ud->nv.name = (uint8_t *)name;
  ud->nv.namelen = namelen;
  ud->nv.value = (uint8_t *)value;
  ud->nv.valuelen = valuelen;

  ud->frame = frame;
  return 0;
}

static int pause_on_header_callback(nghttp2_session *session,
                                    const nghttp2_frame *frame,
                                    const uint8_t *name, size_t namelen,
                                    const uint8_t *value, size_t valuelen,
                                    uint8_t flags, void *user_data) {
  on_header_callback(session, frame, name, namelen, value, valuelen, flags,
                     user_data);
  return NGHTTP2_ERR_PAUSE;
}

static int temporal_failure_on_header_callback(
  nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
  size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
  void *user_data) {
  on_header_callback(session, frame, name, namelen, value, valuelen, flags,
                     user_data);
  return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
}

static int on_invalid_header_callback(nghttp2_session *session,
                                      const nghttp2_frame *frame,
                                      const uint8_t *name, size_t namelen,
                                      const uint8_t *value, size_t valuelen,
                                      uint8_t flags, void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)flags;

  ++ud->invalid_header_cb_called;
  ud->nv.name = (uint8_t *)name;
  ud->nv.namelen = namelen;
  ud->nv.value = (uint8_t *)value;
  ud->nv.valuelen = valuelen;

  ud->frame = frame;
  return 0;
}

static int pause_on_invalid_header_callback(nghttp2_session *session,
                                            const nghttp2_frame *frame,
                                            const uint8_t *name, size_t namelen,
                                            const uint8_t *value,
                                            size_t valuelen, uint8_t flags,
                                            void *user_data) {
  on_invalid_header_callback(session, frame, name, namelen, value, valuelen,
                             flags, user_data);
  return NGHTTP2_ERR_PAUSE;
}

static int reset_on_invalid_header_callback(nghttp2_session *session,
                                            const nghttp2_frame *frame,
                                            const uint8_t *name, size_t namelen,
                                            const uint8_t *value,
                                            size_t valuelen, uint8_t flags,
                                            void *user_data) {
  on_invalid_header_callback(session, frame, name, namelen, value, valuelen,
                             flags, user_data);
  return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
}

static int on_begin_headers_callback(nghttp2_session *session,
                                     const nghttp2_frame *frame,
                                     void *user_data) {
  my_user_data *ud = (my_user_data *)user_data;
  (void)session;
  (void)frame;

  ++ud->begin_headers_cb_called;
  return 0;
}

static int temporal_failure_on_begin_headers_callback(
  nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
  on_begin_headers_callback(session, frame, user_data);
  return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
}

static nghttp2_ssize
defer_data_source_read_callback(nghttp2_session *session, int32_t stream_id,
                                uint8_t *buf, size_t len, uint32_t *data_flags,
                                nghttp2_data_source *source, void *user_data) {
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)len;
  (void)data_flags;
  (void)source;
  (void)user_data;

  return NGHTTP2_ERR_DEFERRED;
}

static int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                                    uint32_t error_code, void *user_data) {
  my_user_data *my_data = (my_user_data *)user_data;
  (void)session;
  (void)stream_id;
  (void)error_code;

  ++my_data->stream_close_cb_called;
  my_data->stream_close_error_code = error_code;

  return 0;
}

static int fatal_error_on_stream_close_callback(nghttp2_session *session,
                                                int32_t stream_id,
                                                uint32_t error_code,
                                                void *user_data) {
  on_stream_close_callback(session, stream_id, error_code, user_data);

  return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static nghttp2_ssize pack_extension_callback(nghttp2_session *session,
                                             uint8_t *buf, size_t len,
                                             const nghttp2_frame *frame,
                                             void *user_data) {
  nghttp2_buf *p = frame->ext.payload;
  (void)session;
  (void)len;
  (void)user_data;

  memcpy(buf, p->pos, nghttp2_buf_len(p));

  return (nghttp2_ssize)nghttp2_buf_len(p);
}

static int on_extension_chunk_recv_callback(nghttp2_session *session,
                                            const nghttp2_frame_hd *hd,
                                            const uint8_t *data, size_t len,
                                            void *user_data) {
  my_user_data *my_data = (my_user_data *)user_data;
  nghttp2_buf *buf = &my_data->scratchbuf;
  (void)session;
  (void)hd;

  buf->last = nghttp2_cpymem(buf->last, data, len);

  return 0;
}

static int cancel_on_extension_chunk_recv_callback(nghttp2_session *session,
                                                   const nghttp2_frame_hd *hd,
                                                   const uint8_t *data,
                                                   size_t len,
                                                   void *user_data) {
  (void)session;
  (void)hd;
  (void)data;
  (void)len;
  (void)user_data;

  return NGHTTP2_ERR_CANCEL;
}

static int unpack_extension_callback(nghttp2_session *session, void **payload,
                                     const nghttp2_frame_hd *hd,
                                     void *user_data) {
  my_user_data *my_data = (my_user_data *)user_data;
  nghttp2_buf *buf = &my_data->scratchbuf;
  (void)session;
  (void)hd;

  *payload = buf;

  return 0;
}

static int cancel_unpack_extension_callback(nghttp2_session *session,
                                            void **payload,
                                            const nghttp2_frame_hd *hd,
                                            void *user_data) {
  (void)session;
  (void)payload;
  (void)hd;
  (void)user_data;

  return NGHTTP2_ERR_CANCEL;
}

static nghttp2_settings_entry *dup_iv(const nghttp2_settings_entry *iv,
                                      size_t niv) {
  return nghttp2_frame_iv_copy(iv, niv, nghttp2_mem_default());
}

static nghttp2_priority_spec pri_spec_default = {0, NGHTTP2_DEFAULT_WEIGHT, 0};

void test_nghttp2_session_recv(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  scripted_data_feed df;
  my_user_data user_data;
  nghttp2_bufs bufs;
  size_t framelen;
  nghttp2_frame frame;
  size_t i;
  nghttp2_outbound_item *item;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_hd_deflater deflater;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.recv_callback2 = scripted_recv_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_begin_frame_callback = on_begin_frame_callback;

  user_data.df = &df;

  nghttp2_session_server_new(&session, &callbacks, &user_data);
  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_int(0, ==, rv);

  scripted_data_feed_init2(&df, &bufs);

  framelen = nghttp2_bufs_len(&bufs);

  /* Send 1 byte per each read */
  for (i = 0; i < framelen; ++i) {
    df.feedseq[i] = 1;
  }

  nghttp2_frame_headers_free(&frame.headers, mem);

  user_data.frame_recv_cb_called = 0;
  user_data.begin_frame_cb_called = 0;

  while (df.seqidx < framelen) {
    assert_int(0, ==, nghttp2_session_recv(session));
  }
  assert_int(1, ==, user_data.frame_recv_cb_called);
  assert_int(1, ==, user_data.begin_frame_cb_called);

  nghttp2_bufs_reset(&bufs);

  /* Receive PRIORITY */
  nghttp2_frame_priority_init(&frame.priority, 5, &pri_spec_default);

  nghttp2_frame_pack_priority(&bufs, &frame.priority);

  nghttp2_frame_priority_free(&frame.priority);

  scripted_data_feed_init2(&df, &bufs);

  user_data.frame_recv_cb_called = 0;
  user_data.begin_frame_cb_called = 0;

  assert_int(0, ==, nghttp2_session_recv(session));
  assert_int(0, ==, user_data.frame_recv_cb_called);
  assert_int(1, ==, user_data.begin_frame_cb_called);

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Some tests for frame too large */
  nghttp2_session_server_new(&session, &callbacks, &user_data);

  /* Receive PING with too large payload */
  nghttp2_frame_ping_init(&frame.ping, NGHTTP2_FLAG_NONE, NULL);

  nghttp2_frame_pack_ping(&bufs, &frame.ping);

  /* Add extra 16 bytes */
  nghttp2_bufs_seek_last_present(&bufs);
  assert(nghttp2_buf_len(&bufs.cur->buf) >= 16);

  bufs.cur->buf.last += 16;
  nghttp2_put_uint32be(
    bufs.cur->buf.pos,
    (uint32_t)(((frame.hd.length + 16) << 8) + bufs.cur->buf.pos[3]));

  nghttp2_frame_ping_free(&frame.ping);

  scripted_data_feed_init2(&df, &bufs);
  user_data.frame_recv_cb_called = 0;
  user_data.begin_frame_cb_called = 0;

  assert_int(0, ==, nghttp2_session_recv(session));
  assert_int(0, ==, user_data.frame_recv_cb_called);
  assert_int(0, ==, user_data.begin_frame_cb_called);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_FRAME_SIZE_ERROR, ==, item->frame.goaway.error_code);
  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_free(&bufs);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_invalid_stream_id(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  scripted_data_feed df;
  my_user_data user_data;
  nghttp2_bufs bufs;
  nghttp2_frame frame;
  nghttp2_hd_deflater deflater;
  int rv;
  nghttp2_mem *mem;
  nghttp2_nv *nva;
  size_t nvlen;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.recv_callback2 = scripted_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  user_data.df = &df;
  user_data.invalid_frame_recv_cb_called = 0;
  nghttp2_session_server_new(&session, &callbacks, &user_data);
  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_int(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  scripted_data_feed_init2(&df, &bufs);
  nghttp2_frame_headers_free(&frame.headers, mem);

  assert_int(0, ==, nghttp2_session_recv(session));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_invalid_frame(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  scripted_data_feed df;
  my_user_data user_data;
  nghttp2_bufs bufs;
  nghttp2_frame frame;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_hd_deflater deflater;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.recv_callback2 = scripted_recv_callback;
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  user_data.df = &df;
  user_data.frame_send_cb_called = 0;
  nghttp2_session_server_new(&session, &callbacks, &user_data);
  nghttp2_hd_deflate_init(&deflater, mem);
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_int(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  scripted_data_feed_init2(&df, &bufs);

  assert_int(0, ==, nghttp2_session_recv(session));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, user_data.frame_send_cb_called);

  /* Receive exactly same bytes of HEADERS is treated as error, because it has
   * pseudo headers and without END_STREAM flag set */
  scripted_data_feed_init2(&df, &bufs);

  assert_int(0, ==, nghttp2_session_recv(session));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, user_data.frame_send_cb_called);
  assert_uint8(NGHTTP2_RST_STREAM, ==, user_data.sent_frame_type);

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_eof(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.recv_callback2 = eof_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);
  assert_int(NGHTTP2_ERR_EOF, ==, nghttp2_session_recv(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  uint8_t data[8092];
  nghttp2_ssize rv;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;
  nghttp2_frame_hd hd;
  int i;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_data_chunk_recv_callback = on_data_chunk_recv_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  /* Create DATA frame with length 4KiB */
  memset(data, 0, sizeof(data));
  hd.length = 4096;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_NONE;
  hd.stream_id = 1;
  nghttp2_frame_pack_frame_hd(data, &hd);

  /* stream 1 is not opened, so it must be responded with connection
     error.  This is not mandated by the spec */
  ud.data_chunk_recv_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);

  assert_int(0, ==, ud.data_chunk_recv_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &ud);

  /* Create stream 1 with CLOSING state. DATA is ignored. */
  stream = open_sent_stream2(session, 1, NGHTTP2_STREAM_CLOSING);

  /* Set initial window size 16383 to check stream flow control,
     isolating it from the connection flow control */
  stream->local_window_size = 16383;

  ud.data_chunk_recv_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);

  assert_int(0, ==, ud.data_chunk_recv_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);
  item = nghttp2_session_get_next_ob_item(session);
  assert_null(item);

  /* This is normal case. DATA is acceptable. */
  stream->state = NGHTTP2_STREAM_OPENED;

  ud.data_chunk_recv_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);

  assert_int(1, ==, ud.data_chunk_recv_cb_called);
  assert_int(1, ==, ud.frame_recv_cb_called);

  assert_null(nghttp2_session_get_next_ob_item(session));

  ud.data_chunk_recv_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);

  /* Now we got data more than initial-window-size / 2, WINDOW_UPDATE
     must be queued */
  assert_int(1, ==, ud.data_chunk_recv_cb_called);
  assert_int(1, ==, ud.frame_recv_cb_called);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.window_update.hd.stream_id);
  assert_int(0, ==, nghttp2_session_send(session));

  /* Set initial window size to 1MiB, so that we can check connection
     flow control individually */
  stream->local_window_size = 1 << 20;
  /* Connection flow control takes into account DATA which is received
     in the error condition. We have received 4096 * 4 bytes of
     DATA. Additional 4 DATA frames, connection flow control will kick
     in. */
  for (i = 0; i < 5; ++i) {
    rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
    assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);
  }
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.window_update.hd.stream_id);
  assert_int(0, ==, nghttp2_session_send(session));

  /* Reception of DATA with stream ID = 0 causes connection error */
  hd.length = 4096;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_NONE;
  hd.stream_id = 0;
  nghttp2_frame_pack_frame_hd(data, &hd);

  ud.data_chunk_recv_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);

  assert_int(0, ==, ud.data_chunk_recv_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_session_del(session);

  /* Check window_update_queued flag in both session and stream */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  hd.length = 4096;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_NONE;
  hd.stream_id = 1;
  nghttp2_frame_pack_frame_hd(data, &hd);

  stream = open_recv_stream(session, 1);

  /* Send 32767 bytes of DATA.  In our current flow control algorithm,
     it triggers first WINDOW_UPDATE of window_size_increment
     32767. */
  for (i = 0; i < 7; ++i) {
    rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
    assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);
  }

  hd.length = 4095;
  nghttp2_frame_pack_frame_hd(data, &hd);
  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4095);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4095, ==, rv);

  /* Now 2 WINDOW_UPDATEs for session and stream should be queued. */
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(0, ==, session->recv_window_size);
  assert_true(stream->window_update_queued);
  assert_true(session->window_update_queued);

  /* Then send 32768 bytes of DATA.  Since we have not sent queued
     WINDOW_UDPATE frame, recv_window_size should not be decreased */
  hd.length = 4096;
  nghttp2_frame_pack_frame_hd(data, &hd);

  for (i = 0; i < 8; ++i) {
    rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
    assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);
  }

  /* WINDOW_UPDATE is blocked for session and stream, so
     recv_window_size must not be decreased. */
  assert_int32(32768, ==, stream->recv_window_size);
  assert_int32(32768, ==, session->recv_window_size);
  assert_true(stream->window_update_queued);
  assert_true(session->window_update_queued);

  ud.frame_send_cb_called = 0;

  /* This sends queued WINDOW_UPDATES.  And then check
     recv_window_size, and queue WINDOW_UPDATEs for both session and
     stream, and send them at once. */
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(4, ==, ud.frame_send_cb_called);
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(0, ==, session->recv_window_size);
  assert_false(stream->window_update_queued);
  assert_false(session->window_update_queued);

  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_data_no_auto_flow_control(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_option *option;
  nghttp2_frame_hd hd;
  size_t padlen;
  uint8_t data[8192];
  nghttp2_ssize rv;
  size_t sendlen;
  nghttp2_stream *stream;
  size_t i;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_window_update(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  /* Create DATA frame with length 4KiB + 11 bytes padding*/
  padlen = 11;
  memset(data, 0, sizeof(data));
  hd.length = 4096 + 1 + padlen;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_PADDED;
  hd.stream_id = 1;
  nghttp2_frame_pack_frame_hd(data, &hd);
  data[NGHTTP2_FRAME_HDLEN] = (uint8_t)padlen;

  /* First create stream 1, then close it.  Check that data is
     consumed for connection in this situation */
  open_recv_stream(session, 1);

  /* Receive first 100 bytes */
  sendlen = 100;
  rv = nghttp2_session_mem_recv2(session, data, sendlen);
  assert_ptrdiff((nghttp2_ssize)sendlen, ==, rv);

  /* We consumed pad length field (1 byte) */
  assert_int32(1, ==, session->consumed_size);

  /* close stream here */
  nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 1, NGHTTP2_NO_ERROR);
  nghttp2_session_send(session);

  /* stream 1 has been closed, and we disabled auto flow-control, so
     data must be immediately consumed for connection. */
  rv = nghttp2_session_mem_recv2(session, data + sendlen,
                                 NGHTTP2_FRAME_HDLEN + hd.length - sendlen);
  assert_ptrdiff((nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + hd.length - sendlen), ==,
                 rv);

  /* We already consumed pad length field (1 byte), so do +1 here */
  assert_int32((int32_t)(NGHTTP2_FRAME_HDLEN + hd.length - sendlen + 1), ==,
               session->consumed_size);

  nghttp2_session_del(session);

  /* Reuse DATA created previously. */

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  /* Now we are expecting final response header, which means receiving
     DATA for that stream is illegal. */
  stream = open_recv_stream(session, 1);
  stream->http_flags |= NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE;

  rv =
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + hd.length);
  assert_ptrdiff((nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + hd.length), ==, rv);

  /* Whole payload must be consumed now because HTTP messaging rule
     was not honored. */
  assert_int32((int32_t)hd.length, ==, session->consumed_size);

  nghttp2_session_del(session);

  /* Check window_update_queued flag in both session and stream */
  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  stream = open_recv_stream(session, 1);

  hd.length = 4096;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_NONE;
  hd.stream_id = 1;
  nghttp2_frame_pack_frame_hd(data, &hd);

  /* Receive up to 65535 bytes of DATA */
  for (i = 0; i < 15; ++i) {
    rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4096);
    assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4096, ==, rv);
  }

  hd.length = 4095;
  nghttp2_frame_pack_frame_hd(data, &hd);

  rv = nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 4095);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 4095, ==, rv);

  assert_int32(65535, ==, session->recv_window_size);
  assert_int32(65535, ==, stream->recv_window_size);

  /* The first call of nghttp2_session_consume_connection() will queue
     WINDOW_UPDATE.  Next call does not. */
  nghttp2_session_consume_connection(session, 32767);
  nghttp2_session_consume_connection(session, 32768);

  assert_int32(32768, ==, session->recv_window_size);
  assert_int32(65535, ==, stream->recv_window_size);
  assert_true(session->window_update_queued);
  assert_false(stream->window_update_queued);

  ud.frame_send_cb_called = 0;

  /* This will send WINDOW_UPDATE, and check whether we should send
     WINDOW_UPDATE, and queue and send it at once. */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(0, ==, session->recv_window_size);
  assert_int32(65535, ==, stream->recv_window_size);
  assert_false(session->window_update_queued);
  assert_false(stream->window_update_queued);
  assert_int(2, ==, ud.frame_send_cb_called);

  /* Do the same for stream */
  nghttp2_session_consume_stream(session, 1, 32767);
  nghttp2_session_consume_stream(session, 1, 32768);

  assert_int32(0, ==, session->recv_window_size);
  assert_int32(32768, ==, stream->recv_window_size);
  assert_false(session->window_update_queued);
  assert_true(stream->window_update_queued);

  ud.frame_send_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(0, ==, session->recv_window_size);
  assert_int32(0, ==, stream->recv_window_size);
  assert_false(session->window_update_queued);
  assert_false(stream->window_update_queued);
  assert_int(2, ==, ud.frame_send_cb_called);

  nghttp2_session_del(session);
  nghttp2_option_del(option);
}

void test_nghttp2_session_recv_continuation(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_hd_deflater deflater;
  uint8_t data[1024];
  size_t datalen;
  nghttp2_frame_hd cont_hd;
  nghttp2_priority_spec pri_spec;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_header_callback = on_header_callback;
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_begin_frame_callback = on_begin_frame_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* Make 1 HEADERS and insert CONTINUATION header */
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_NONE, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  /* make sure that all data is in the first buf */
  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* HEADERS's payload is 1 byte */
  memcpy(data, buf->pos, NGHTTP2_FRAME_HDLEN + 1);
  datalen = NGHTTP2_FRAME_HDLEN + 1;
  buf->pos += NGHTTP2_FRAME_HDLEN + 1;

  nghttp2_put_uint32be(data, (uint32_t)((1 << 8) + data[3]));

  /* First CONTINUATION, 2 bytes */
  nghttp2_frame_hd_init(&cont_hd, 2, NGHTTP2_CONTINUATION, NGHTTP2_FLAG_NONE,
                        1);

  nghttp2_frame_pack_frame_hd(data + datalen, &cont_hd);
  datalen += NGHTTP2_FRAME_HDLEN;

  memcpy(data + datalen, buf->pos, cont_hd.length);
  datalen += cont_hd.length;
  buf->pos += cont_hd.length;

  /* Second CONTINUATION, rest of the bytes */
  nghttp2_frame_hd_init(&cont_hd, nghttp2_buf_len(buf), NGHTTP2_CONTINUATION,
                        NGHTTP2_FLAG_END_HEADERS, 1);

  nghttp2_frame_pack_frame_hd(data + datalen, &cont_hd);
  datalen += NGHTTP2_FRAME_HDLEN;

  memcpy(data + datalen, buf->pos, cont_hd.length);
  datalen += cont_hd.length;
  buf->pos += cont_hd.length;

  assert_size(0, ==, nghttp2_buf_len(buf));

  ud.header_cb_called = 0;
  ud.begin_frame_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, data, datalen);
  assert_ptrdiff((nghttp2_ssize)datalen, ==, rv);
  assert_int(4, ==, ud.header_cb_called);
  assert_int(3, ==, ud.begin_frame_cb_called);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* HEADERS with padding followed by CONTINUATION */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_NONE, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);

  nghttp2_bufs_reset(&bufs);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* make sure that all data is in the first buf */
  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  /* HEADERS payload is 3 byte (1 for padding field, 1 for padding) */
  memcpy(data, buf->pos, NGHTTP2_FRAME_HDLEN);
  nghttp2_put_uint32be(data, (uint32_t)((3 << 8) + data[3]));
  data[4] |= NGHTTP2_FLAG_PADDED;
  /* padding field */
  data[NGHTTP2_FRAME_HDLEN] = 1;
  data[NGHTTP2_FRAME_HDLEN + 1] = buf->pos[NGHTTP2_FRAME_HDLEN];
  /* padding */
  data[NGHTTP2_FRAME_HDLEN + 2] = 0;
  datalen = NGHTTP2_FRAME_HDLEN + 3;
  buf->pos += NGHTTP2_FRAME_HDLEN + 1;

  /* CONTINUATION, rest of the bytes */
  nghttp2_frame_hd_init(&cont_hd, nghttp2_buf_len(buf), NGHTTP2_CONTINUATION,
                        NGHTTP2_FLAG_END_HEADERS, 1);
  nghttp2_frame_pack_frame_hd(data + datalen, &cont_hd);
  datalen += NGHTTP2_FRAME_HDLEN;

  memcpy(data + datalen, buf->pos, cont_hd.length);
  datalen += cont_hd.length;
  buf->pos += cont_hd.length;

  assert_size(0, ==, nghttp2_buf_len(buf));

  ud.header_cb_called = 0;
  ud.begin_frame_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, data, datalen);

  assert_ptrdiff((nghttp2_ssize)datalen, ==, rv);
  assert_int(4, ==, ud.header_cb_called);
  assert_int(2, ==, ud.begin_frame_cb_called);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Expecting CONTINUATION, but get the other frame */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* HEADERS without END_HEADERS flag */
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_NONE, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  nghttp2_bufs_reset(&bufs);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* make sure that all data is in the first buf */
  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  memcpy(data, buf->pos, nghttp2_buf_len(buf));
  datalen = nghttp2_buf_len(buf);

  /* Followed by PRIORITY */
  nghttp2_priority_spec_default_init(&pri_spec);

  nghttp2_frame_priority_init(&frame.priority, 1, &pri_spec);
  nghttp2_bufs_reset(&bufs);

  nghttp2_frame_pack_priority(&bufs, &frame.priority);

  assert_size(0, <, nghttp2_bufs_len(&bufs));

  memcpy(data + datalen, buf->pos, nghttp2_buf_len(buf));
  datalen += nghttp2_buf_len(buf);

  ud.begin_headers_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, data, datalen);
  assert_ptrdiff((nghttp2_ssize)datalen, ==, rv);

  assert_int(1, ==, ud.begin_headers_cb_called);
  assert_uint8(NGHTTP2_GOAWAY, ==,
               nghttp2_session_get_next_ob_item(session)->frame.hd.type);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_headers_with_priority(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_hd_deflater deflater;
  nghttp2_outbound_item *item;
  nghttp2_priority_spec pri_spec;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  open_recv_stream(session, 1);

  /* With NGHTTP2_FLAG_PRIORITY without exclusive flag set */
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);

  nghttp2_priority_spec_init(&pri_spec, 1, 99, 0);

  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             3, NGHTTP2_HCAT_HEADERS, &pri_spec, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  stream = nghttp2_session_get_stream(session, 3);

  assert_not_null(stream);

  nghttp2_bufs_reset(&bufs);

  /* With NGHTTP2_FLAG_PRIORITY, but cut last 1 byte to make it
     invalid. */
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);

  nghttp2_priority_spec_init(&pri_spec, 0, 99, 0);

  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             5, NGHTTP2_HCAT_HEADERS, &pri_spec, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN + 5, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  /* Make payload shorter than required length to store priority
     group */
  nghttp2_put_uint32be(buf->pos, (uint32_t)((4 << 8) + buf->pos[3]));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  stream = nghttp2_session_get_stream(session, 5);

  assert_null(stream);

  item = nghttp2_session_get_next_ob_item(session);
  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_FRAME_SIZE_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Check dep_stream_id == stream_id */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);

  nghttp2_priority_spec_init(&pri_spec, 1, 0, 0);

  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             1, NGHTTP2_HCAT_HEADERS, &pri_spec, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  stream = nghttp2_session_get_stream(session, 1);

  assert_null(stream);

  item = nghttp2_session_get_next_ob_item(session);
  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_bufs_reset(&bufs);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_headers_with_padding(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_frame_hd hd;
  nghttp2_outbound_item *item;
  my_user_data ud;
  nghttp2_ssize rv;

  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.send_callback2 = null_send_callback;

  /* HEADERS: Wrong padding length */
  nghttp2_session_server_new(&session, &callbacks, &ud);
  nghttp2_session_send(session);

  nghttp2_frame_hd_init(
    &hd, 10, NGHTTP2_HEADERS,
    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY | NGHTTP2_FLAG_PADDED, 1);
  buf = &bufs.head->buf;
  nghttp2_frame_pack_frame_hd(buf->last, &hd);
  buf->last += NGHTTP2_FRAME_HDLEN;
  /* padding is 6 bytes */
  *buf->last++ = 5;
  /* priority field */
  nghttp2_put_uint32be(buf->last, 3);
  buf->last += sizeof(uint32_t);
  *buf->last++ = 1;
  /* rest is garbage */
  memset(buf->last, 0, 4);
  buf->last += 4;

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_bufs_reset(&bufs);
  nghttp2_session_del(session);

  /* PUSH_PROMISE: Wrong padding length */
  nghttp2_session_client_new(&session, &callbacks, &ud);
  nghttp2_session_send(session);

  open_sent_stream(session, 1);

  nghttp2_frame_hd_init(&hd, 9, NGHTTP2_PUSH_PROMISE,
                        NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PADDED, 1);
  buf = &bufs.head->buf;
  nghttp2_frame_pack_frame_hd(buf->last, &hd);
  buf->last += NGHTTP2_FRAME_HDLEN;
  /* padding is 6 bytes */
  *buf->last++ = 5;
  /* promised stream ID field */
  nghttp2_put_uint32be(buf->last, 2);
  buf->last += sizeof(uint32_t);
  /* rest is garbage */
  memset(buf->last, 0, 4);
  buf->last += 4;

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_bufs_free(&bufs);
  nghttp2_session_del(session);
}

static int response_on_begin_frame_callback(nghttp2_session *session,
                                            const nghttp2_frame_hd *hd,
                                            void *user_data) {
  int rv;
  (void)user_data;

  if (hd->type != NGHTTP2_HEADERS) {
    return 0;
  }

  rv = nghttp2_submit_response2(session, hd->stream_id, resnv, ARRLEN(resnv),
                                NULL);

  assert_int(0, ==, rv);

  return 0;
}

void test_nghttp2_session_recv_headers_early_response(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_frame frame;
  nghttp2_ssize rv;
  nghttp2_stream *stream;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_begin_frame_callback = response_on_begin_frame_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                             1, NGHTTP2_HCAT_REQUEST, NULL, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;

  /* Only receive 9 bytes headers, and invoke
     on_begin_frame_callback */
  rv = nghttp2_session_mem_recv2(session, buf->pos, 9);

  assert_ptrdiff(9, ==, rv);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  rv =
    nghttp2_session_mem_recv2(session, buf->pos + 9, nghttp2_buf_len(buf) - 9);

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf) - 9, ==, rv);

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_null(stream);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_recv_headers_for_closed_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_hd_deflater deflater;
  nghttp2_stream *stream;
  nghttp2_mem *mem;
  const uint8_t *data;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_header_callback = on_header_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* Make sure that on_header callback never be invoked for closed
     stream */
  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.header_cb_called = 0;
  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, NGHTTP2_FRAME_HDLEN);

  assert_ptrdiff(NGHTTP2_FRAME_HDLEN, ==, rv);
  assert_int(0, ==, ud.header_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);

  stream = nghttp2_session_get_stream(session, 1);

  assert_not_null(stream);

  rv =
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 1, NGHTTP2_NO_ERROR);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, rv);

  stream = nghttp2_session_get_stream(session, 1);

  assert_null(stream);

  ud.header_cb_called = 0;
  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos + NGHTTP2_FRAME_HDLEN,
                                 nghttp2_buf_len(buf) - NGHTTP2_FRAME_HDLEN);

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf) - NGHTTP2_FRAME_HDLEN, ==,
                 rv);
  assert_int(0, ==, ud.header_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_headers_with_extpri(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  nghttp2_hd_deflater deflater;
  nghttp2_stream *stream;
  nghttp2_mem *mem;
  const nghttp2_nv extpri_reqnv[] = {
    MAKE_NV(":method", "GET"),    MAKE_NV(":path", "/"),
    MAKE_NV(":scheme", "https"),  MAKE_NV(":authority", "localhost"),
    MAKE_NV("priority", "i,u=2"),
  };
  nghttp2_settings_entry iv;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_server_new(&session, &callbacks, NULL);

  iv.settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv.value = 1;

  nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1);

  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(extpri_reqnv);
  nghttp2_nv_array_copy(&nva, extpri_reqnv, nvlen, mem);

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);

  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  stream = nghttp2_session_get_stream(session, 1);

  assert_uint32(2, ==, nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_bufs_reset(&bufs);

  /* Client should ignore priority header field included in
     PUSH_PROMISE. */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1);

  open_sent_stream(session, 1);

  nghttp2_hd_deflate_init(&deflater, mem);

  nvlen = ARRLEN(extpri_reqnv);
  nghttp2_nv_array_copy(&nva, extpri_reqnv, nvlen, mem);

  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 2, nva, nvlen);

  rv = nghttp2_frame_pack_push_promise(&bufs, &frame.push_promise, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  stream = nghttp2_session_get_stream(session, 2);

  assert_uint32(NGHTTP2_EXTPRI_DEFAULT_URGENCY, ==,
                nghttp2_extpri_uint8_urgency(stream->http_extpri));
  assert_uint32(NGHTTP2_EXTPRI_DEFAULT_URGENCY, ==,
                nghttp2_extpri_uint8_urgency(stream->extpri));

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_server_recv_push_response(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_mem *mem;
  nghttp2_frame frame;
  nghttp2_hd_deflater deflater;
  nghttp2_nv *nva;
  size_t nvlen;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  nvlen = ARRLEN(resnv);
  nghttp2_nv_array_copy(&nva, resnv, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_HEADERS, &pri_spec_default, nva,
                             nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;

  ud.invalid_frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(1, ==, ud.invalid_frame_recv_cb_called);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_premature_headers(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_hd_deflater deflater;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;
  uint32_t payloadlen;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, reqnv,
               ARRLEN(reqnv), mem);

  buf = &bufs.head->buf;
  /* Intentionally feed payload cutting last 1 byte off */
  payloadlen = nghttp2_get_uint32(buf->pos) >> 8;
  nghttp2_put_uint32be(buf->pos, ((payloadlen - 1) << 8) + buf->pos[3]);
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf) - 1);

  assert_ptrdiff((nghttp2_ssize)(nghttp2_buf_len(buf) - 1), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_COMPRESSION_ERROR, ==,
                item->frame.rst_stream.error_code);
  assert_int32(1, ==, item->frame.hd.stream_id);
  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Test for PUSH_PROMISE */
  nghttp2_session_client_new(&session, &callbacks, &ud);
  nghttp2_hd_deflate_init(&deflater, mem);

  open_sent_stream3(session, 1, NGHTTP2_STREAM_FLAG_NONE,
                    NGHTTP2_STREAM_OPENING, NULL);

  rv = pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 2,
                         reqnv, ARRLEN(reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  buf = &bufs.head->buf;
  payloadlen = nghttp2_get_uint32(buf->pos) >> 8;
  /* Intentionally feed payload cutting last 1 byte off */
  nghttp2_put_uint32be(buf->pos, ((payloadlen - 1) << 8) + buf->pos[3]);
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf) - 1);

  assert_ptrdiff((nghttp2_ssize)(nghttp2_buf_len(buf) - 1), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_COMPRESSION_ERROR, ==,
                item->frame.rst_stream.error_code);
  assert_int32(2, ==, item->frame.hd.stream_id);
  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_recv_unknown_frame(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  uint8_t data[16384];
  size_t datalen;
  nghttp2_frame_hd hd;
  nghttp2_ssize rv;

  nghttp2_frame_hd_init(&hd, 16000, 99, NGHTTP2_FLAG_NONE, 0);

  nghttp2_frame_pack_frame_hd(data, &hd);
  datalen = NGHTTP2_FRAME_HDLEN + hd.length;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  ud.frame_recv_cb_called = 0;

  /* Unknown frame must be ignored */
  rv = nghttp2_session_mem_recv2(session, data, datalen);

  assert_ptrdiff(rv, ==, (nghttp2_ssize)datalen);
  assert_int(0, ==, ud.frame_recv_cb_called);
  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_unexpected_continuation(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  uint8_t data[16384];
  size_t datalen;
  nghttp2_frame_hd hd;
  nghttp2_ssize rv;
  nghttp2_outbound_item *item;

  nghttp2_frame_hd_init(&hd, 16000, NGHTTP2_CONTINUATION,
                        NGHTTP2_FLAG_END_HEADERS, 1);

  nghttp2_frame_pack_frame_hd(data, &hd);
  datalen = NGHTTP2_FRAME_HDLEN + hd.length;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 1);

  ud.frame_recv_cb_called = 0;

  /* unexpected CONTINUATION must be treated as connection error */
  rv = nghttp2_session_mem_recv2(session, data, datalen);

  assert_ptrdiff(rv, ==, (nghttp2_ssize)datalen);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_settings_header_table_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_settings_entry iv[3];
  nghttp2_nv nv = MAKE_NV(":authority", "example.org");
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 3000;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16384;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 2),
                              2);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  assert_uint32(3000, ==, session->remote_settings.header_table_size);
  assert_uint32(16384, ==, session->remote_settings.initial_window_size);

  nghttp2_bufs_reset(&bufs);

  /* 2 SETTINGS_HEADER_TABLE_SIZE */
  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 3001;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16383;

  iv[2].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[2].value = 3001;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 3),
                              3);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)(nghttp2_buf_len(buf)), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  assert_uint32(3001, ==, session->remote_settings.header_table_size);
  assert_uint32(16383, ==, session->remote_settings.initial_window_size);

  nghttp2_bufs_reset(&bufs);

  /* 2 SETTINGS_HEADER_TABLE_SIZE; first entry clears dynamic header
     table. */

  nghttp2_submit_request2(session, NULL, &nv, 1, NULL, NULL);
  nghttp2_session_send(session);

  assert_size(0, <, session->hd_deflater.ctx.hd_table.len);

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 0;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16382;

  iv[2].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[2].value = 4096;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 3),
                              3);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  assert_uint32(4096, ==, session->remote_settings.header_table_size);
  assert_uint32(16382, ==, session->remote_settings.initial_window_size);
  assert_size(0, ==, session->hd_deflater.ctx.hd_table.len);

  nghttp2_bufs_reset(&bufs);

  /* 2 SETTINGS_HEADER_TABLE_SIZE; second entry clears dynamic header
     table. */

  nghttp2_submit_request2(session, NULL, &nv, 1, NULL, NULL);
  nghttp2_session_send(session);

  assert_size(0, <, session->hd_deflater.ctx.hd_table.len);

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 3000;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16381;

  iv[2].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[2].value = 0;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 3),
                              3);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  assert_uint32(0, ==, session->remote_settings.header_table_size);
  assert_uint32(16381, ==, session->remote_settings.initial_window_size);
  assert_size(0, ==, session->hd_deflater.ctx.hd_table.len);

  nghttp2_bufs_reset(&bufs);

  nghttp2_bufs_free(&bufs);
  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_too_large_frame_length(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  uint8_t buf[NGHTTP2_FRAME_HDLEN];
  nghttp2_outbound_item *item;
  nghttp2_frame_hd hd;

  /* Initial max frame size is NGHTTP2_MAX_FRAME_SIZE_MIN */
  nghttp2_frame_hd_init(&hd, NGHTTP2_MAX_FRAME_SIZE_MIN + 1, NGHTTP2_HEADERS,
                        NGHTTP2_FLAG_NONE, 1);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_frame_pack_frame_hd(buf, &hd);

  assert_ptrdiff(sizeof(buf), ==,
                 nghttp2_session_mem_recv2(session, buf, sizeof(buf)));

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);
}

void test_nghttp2_session_recv_extension(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_buf buf;
  nghttp2_frame_hd hd;
  nghttp2_mem *mem;
  const char data[] = "Hello World!";
  nghttp2_ssize rv;
  nghttp2_option *option;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  callbacks.on_extension_chunk_recv_callback = on_extension_chunk_recv_callback;
  callbacks.unpack_extension_callback = unpack_extension_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_user_recv_extension_type(option, 111);

  nghttp2_buf_init2(&ud.scratchbuf, 4096, mem);
  nghttp2_buf_init2(&buf, 4096, mem);

  nghttp2_frame_hd_init(&hd, sizeof(data), 111, 0xab, 1000000007);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  buf.last = nghttp2_cpymem(buf.last, data, sizeof(data));

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&ud.recv_frame_hd, 0, 0, 0, 0);
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_size(NGHTTP2_FRAME_HDLEN + hd.length, ==, (size_t)rv);
  assert_uint8(111, ==, ud.recv_frame_hd.type);
  assert_uint8(0xab, ==, ud.recv_frame_hd.flags);
  assert_int32(1000000007, ==, ud.recv_frame_hd.stream_id);
  assert_memory_equal(sizeof(data), data, ud.scratchbuf.pos);

  nghttp2_session_del(session);

  /* cancel in on_extension_chunk_recv_callback */
  nghttp2_buf_reset(&ud.scratchbuf);

  callbacks.on_extension_chunk_recv_callback =
    cancel_on_extension_chunk_recv_callback;

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_size(NGHTTP2_FRAME_HDLEN + hd.length, ==, (size_t)rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* cancel in unpack_extension_callback */
  nghttp2_buf_reset(&ud.scratchbuf);

  callbacks.on_extension_chunk_recv_callback = on_extension_chunk_recv_callback;
  callbacks.unpack_extension_callback = cancel_unpack_extension_callback;

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_size(NGHTTP2_FRAME_HDLEN + hd.length, ==, (size_t)rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  nghttp2_buf_free(&buf, mem);
  nghttp2_buf_free(&ud.scratchbuf, mem);

  nghttp2_option_del(option);
}

void test_nghttp2_session_recv_altsvc(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_buf buf;
  nghttp2_frame_hd hd;
  nghttp2_mem *mem;
  nghttp2_ssize rv;
  nghttp2_option *option;
  static const uint8_t origin[] = "nghttp2.org";
  static const uint8_t field_value[] = "h2=\":443\"";

  mem = nghttp2_mem_default();

  nghttp2_buf_init2(&buf, NGHTTP2_FRAME_HDLEN + NGHTTP2_MAX_FRAME_SIZE_MIN,
                    mem);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_builtin_recv_extension_type(option, NGHTTP2_ALTSVC);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(origin) - 1 + sizeof(field_value) - 1,
                        NGHTTP2_ALTSVC, NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  buf.last = nghttp2_cpymem(buf.last, field_value, sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_ALTSVC, ==, ud.recv_frame_hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, ud.recv_frame_hd.flags);
  assert_int32(0, ==, ud.recv_frame_hd.stream_id);

  nghttp2_session_del(session);

  /* size of origin is larger than frame length */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(origin) - 1 - 1, NGHTTP2_ALTSVC,
                        NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1 - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* zero-length value */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(origin) - 1, NGHTTP2_ALTSVC,
                        NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);

  ud.invalid_frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(1, ==, ud.invalid_frame_recv_cb_called);

  nghttp2_session_del(session);

  /* non-empty origin to a stream other than 0 */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  open_sent_stream(session, 1);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(origin) - 1 + sizeof(field_value) - 1,
                        NGHTTP2_ALTSVC, NGHTTP2_FLAG_NONE, 1);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  buf.last = nghttp2_cpymem(buf.last, field_value, sizeof(field_value) - 1);

  ud.invalid_frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(1, ==, ud.invalid_frame_recv_cb_called);

  nghttp2_session_del(session);

  /* empty origin to stream 0 */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(field_value) - 1, NGHTTP2_ALTSVC,
                        NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, 0);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, field_value, sizeof(field_value) - 1);

  ud.invalid_frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(1, ==, ud.invalid_frame_recv_cb_called);

  nghttp2_session_del(session);

  /* send large frame (16KiB) */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, NGHTTP2_MAX_FRAME_SIZE_MIN, NGHTTP2_ALTSVC,
                        NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  memset(buf.last, 0, nghttp2_buf_avail(&buf));
  buf.last += nghttp2_buf_avail(&buf);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_ALTSVC, ==, ud.recv_frame_hd.type);
  assert_size(NGHTTP2_MAX_FRAME_SIZE_MIN, ==, ud.recv_frame_hd.length);

  nghttp2_session_del(session);

  /* send too large frame */
  nghttp2_buf_reset(&buf);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  session->local_settings.max_frame_size = NGHTTP2_MAX_FRAME_SIZE_MIN - 1;

  nghttp2_frame_hd_init(&hd, NGHTTP2_MAX_FRAME_SIZE_MIN + 1, NGHTTP2_ALTSVC,
                        NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  memset(buf.last, 0, nghttp2_buf_avail(&buf));
  buf.last += nghttp2_buf_avail(&buf);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* received by server */
  nghttp2_buf_reset(&buf);

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_hd_init(&hd, 2 + sizeof(origin) - 1 + sizeof(field_value) - 1,
                        NGHTTP2_ALTSVC, NGHTTP2_FLAG_NONE, 0);
  nghttp2_frame_pack_frame_hd(buf.last, &hd);
  buf.last += NGHTTP2_FRAME_HDLEN;
  nghttp2_put_uint16be(buf.last, sizeof(origin) - 1);
  buf.last += 2;
  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  buf.last = nghttp2_cpymem(buf.last, field_value, sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf.pos, nghttp2_buf_len(&buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&buf), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  nghttp2_buf_free(&buf, mem);
  nghttp2_option_del(option);
}

void test_nghttp2_session_recv_origin(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  nghttp2_option *option;
  nghttp2_extension frame;
  nghttp2_ext_origin origin;
  nghttp2_origin_entry ov;
  static const uint8_t nghttp2[] = "https://nghttp2.org";

  frame_pack_bufs_init(&bufs);

  frame.payload = &origin;

  ov.origin = (uint8_t *)nghttp2;
  ov.origin_len = sizeof(nghttp2) - 1;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_builtin_recv_extension_type(option, NGHTTP2_ORIGIN);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, &ov, 1);

  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_ORIGIN, ==, ud.recv_frame_hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, ud.recv_frame_hd.flags);
  assert_int32(0, ==, ud.recv_frame_hd.stream_id);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* The length of origin is larger than payload length. */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, &ov, 1);
  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  nghttp2_put_uint16be(bufs.head->buf.pos + NGHTTP2_FRAME_HDLEN,
                       (uint16_t)sizeof(nghttp2));

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* A frame should be ignored if it is sent to a stream other than
     stream 0. */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, &ov, 1);
  frame.hd.stream_id = 1;
  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* A frame should be ignored if the reserved flag is set */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, &ov, 1);
  frame.hd.flags = 0xf0;
  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* A frame should be ignored if it is received by a server. */
  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, &ov, 1);
  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* Receiving empty ORIGIN frame */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  nghttp2_frame_origin_init(&frame, NULL, 0);
  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_ORIGIN, ==, ud.recv_frame_hd.type);

  nghttp2_session_del(session);

  nghttp2_option_del(option);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_recv_priority_update(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  nghttp2_option *option;
  nghttp2_extension frame;
  nghttp2_ext_priority_update priority_update;
  nghttp2_stream *stream;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  uint8_t large_field_value[sizeof(session->iframe.raw_sbuf) + 1];
  nghttp2_outbound_item *item;
  size_t i;
  int32_t stream_id;
  static const uint8_t field_value[] = "u=2,i";

  mem = nghttp2_mem_default();

  memset(large_field_value, ' ', sizeof(large_field_value));
  memcpy(large_field_value, field_value, sizeof(field_value) - 1);

  frame_pack_bufs_init(&bufs);

  frame.payload = &priority_update;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_builtin_recv_extension_type(option,
                                                 NGHTTP2_PRIORITY_UPDATE);

  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  session->pending_no_rfc7540_priorities = 1;

  nghttp2_frame_priority_update_init(&frame, 1, (uint8_t *)field_value,
                                     sizeof(field_value) - 1);

  nghttp2_frame_pack_priority_update(&bufs, &frame);

  open_recv_stream(session, 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_PRIORITY_UPDATE, ==, ud.recv_frame_hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, ud.recv_frame_hd.flags);
  assert_int32(0, ==, ud.recv_frame_hd.stream_id);

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_uint32(2, ==, nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* Check that priority which is received in idle state is
     retained. */
  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  session->pending_no_rfc7540_priorities = 1;

  nghttp2_frame_priority_update_init(&frame, 1, (uint8_t *)field_value,
                                     sizeof(field_value) - 1);

  nghttp2_frame_pack_priority_update(&bufs, &frame);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_PRIORITY_UPDATE, ==, ud.recv_frame_hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, ud.recv_frame_hd.flags);
  assert_int32(0, ==, ud.recv_frame_hd.stream_id);

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_IDLE, ==, stream->state);
  assert_uint32(2, ==, nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_bufs_reset(&bufs);
  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.recv_frame_hd.type);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);
  assert_uint32(2, ==, nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* PRIORITY_UPDATE with too large field_value is discarded */
  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  session->pending_no_rfc7540_priorities = 1;

  nghttp2_frame_priority_update_init(&frame, 1, large_field_value,
                                     sizeof(large_field_value));

  nghttp2_frame_pack_priority_update(&bufs, &frame);

  open_recv_stream(session, 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_uint32(NGHTTP2_EXTPRI_DEFAULT_URGENCY, ==, stream->extpri);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* Connection error if client receives PRIORITY_UPDATE. */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  session->pending_no_rfc7540_priorities = 1;

  nghttp2_frame_priority_update_init(&frame, 1, (uint8_t *)field_value,
                                     sizeof(field_value) - 1);

  nghttp2_frame_pack_priority_update(&bufs, &frame);

  open_sent_stream(session, 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_session_del(session);
  nghttp2_bufs_reset(&bufs);

  /* The number of idle streams exceeds the maximum. */
  nghttp2_session_server_new2(&session, &callbacks, &ud, option);

  session->pending_no_rfc7540_priorities = 1;
  session->local_settings.max_concurrent_streams = 100;

  for (i = 0; i < 101; ++i) {
    stream_id = (int32_t)(i * 2 + 1);
    nghttp2_frame_priority_update_init(
      &frame, stream_id, (uint8_t *)field_value, sizeof(field_value) - 1);

    nghttp2_frame_pack_priority_update(&bufs, &frame);

    ud.frame_recv_cb_called = 0;
    rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                   nghttp2_bufs_len(&bufs));

    if (i < 100) {
      assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
      assert_int(1, ==, ud.frame_recv_cb_called);
      assert_uint8(NGHTTP2_PRIORITY_UPDATE, ==, ud.recv_frame_hd.type);
    } else {
      assert_int(0, ==, ud.frame_recv_cb_called);
    }

    nghttp2_bufs_reset(&bufs);
  }

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_session_del(session);
  nghttp2_option_del(option);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_continue(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  const nghttp2_nv nv1[] = {MAKE_NV(":method", "GET"), MAKE_NV(":path", "/")};
  const nghttp2_nv nv2[] = {MAKE_NV("user-agent", "nghttp2/1.0.0"),
                            MAKE_NV("alpha", "bravo")};
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  size_t framelen1, framelen2;
  nghttp2_ssize rv;
  uint8_t buffer[4096];
  nghttp2_buf databuf;
  nghttp2_frame frame;
  nghttp2_nv *nva;
  size_t nvlen;
  const nghttp2_frame *recv_frame;
  nghttp2_frame_hd data_hd;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);
  nghttp2_buf_wrap_init(&databuf, buffer, sizeof(buffer));

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_data_chunk_recv_callback = pause_on_data_chunk_recv_callback;
  callbacks.on_header_callback = pause_on_header_callback;
  callbacks.on_begin_headers_callback = on_begin_headers_callback;

  nghttp2_session_server_new(&session, &callbacks, &user_data);
  /* disable strict HTTP layering checks */
  session->opt_flags |= NGHTTP2_OPTMASK_NO_HTTP_MESSAGING;

  nghttp2_hd_deflate_init(&deflater, mem);

  /* Make 2 HEADERS frames */
  nvlen = ARRLEN(nv1);
  nghttp2_nv_array_copy(&nva, nv1, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  framelen1 = nghttp2_buf_len(buf);
  databuf.last = nghttp2_cpymem(databuf.last, buf->pos, nghttp2_buf_len(buf));

  nvlen = ARRLEN(nv2);
  nghttp2_nv_array_copy(&nva, nv2, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 3,
                             NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  nghttp2_bufs_reset(&bufs);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_headers_free(&frame.headers, mem);

  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  framelen2 = nghttp2_buf_len(buf);
  databuf.last = nghttp2_cpymem(databuf.last, buf->pos, nghttp2_buf_len(buf));

  /* Receive 1st HEADERS and pause */
  user_data.begin_headers_cb_called = 0;
  user_data.header_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(0, <=, rv);
  databuf.pos += rv;

  recv_frame = user_data.frame;
  assert_uint8(NGHTTP2_HEADERS, ==, recv_frame->hd.type);
  assert_size(framelen1 - NGHTTP2_FRAME_HDLEN, ==, recv_frame->hd.length);

  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.header_cb_called);

  assert_true(nghttp2_nv_equal(&nv1[0], &user_data.nv));

  /* get 2nd header field */
  user_data.begin_headers_cb_called = 0;
  user_data.header_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(0, <=, rv);
  databuf.pos += rv;

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.header_cb_called);

  assert_true(nghttp2_nv_equal(&nv1[1], &user_data.nv));

  /* will call end_headers_callback and receive 2nd HEADERS and pause */
  user_data.begin_headers_cb_called = 0;
  user_data.header_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(0, <=, rv);
  databuf.pos += rv;

  recv_frame = user_data.frame;
  assert_uint8(NGHTTP2_HEADERS, ==, recv_frame->hd.type);
  assert_size(framelen2 - NGHTTP2_FRAME_HDLEN, ==, recv_frame->hd.length);

  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.header_cb_called);

  assert_true(nghttp2_nv_equal(&nv2[0], &user_data.nv));

  /* get 2nd header field */
  user_data.begin_headers_cb_called = 0;
  user_data.header_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(0, <=, rv);
  databuf.pos += rv;

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.header_cb_called);

  assert_true(nghttp2_nv_equal(&nv2[1], &user_data.nv));

  /* No input data, frame_recv_callback is called */
  user_data.begin_headers_cb_called = 0;
  user_data.header_cb_called = 0;
  user_data.frame_recv_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(0, <=, rv);
  databuf.pos += rv;

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(0, ==, user_data.header_cb_called);
  assert_int(1, ==, user_data.frame_recv_cb_called);

  /* Receive DATA */
  nghttp2_frame_hd_init(&data_hd, 16, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 1);

  nghttp2_buf_reset(&databuf);
  nghttp2_frame_pack_frame_hd(databuf.pos, &data_hd);

  /* Intentionally specify larger buffer size to see pause is kicked
     in. */
  databuf.last = databuf.end;

  user_data.frame_recv_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));

  assert_ptrdiff(16 + NGHTTP2_FRAME_HDLEN, ==, rv);
  assert_int(0, ==, user_data.frame_recv_cb_called);

  /* Next nghttp2_session_mem_recv2 invokes on_frame_recv_callback and
     pause again in on_data_chunk_recv_callback since we pass same
     DATA frame. */
  user_data.frame_recv_cb_called = 0;
  rv =
    nghttp2_session_mem_recv2(session, databuf.pos, nghttp2_buf_len(&databuf));
  assert_ptrdiff(16 + NGHTTP2_FRAME_HDLEN, ==, rv);
  assert_int(1, ==, user_data.frame_recv_cb_called);

  /* And finally call on_frame_recv_callback with 0 size input */
  user_data.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, NULL, 0);
  assert_ptrdiff(0, ==, rv);
  assert_int(1, ==, user_data.frame_recv_cb_called);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
}

void test_nghttp2_session_add_frame(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  accumulator acc;
  my_user_data user_data;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;

  acc.length = 0;
  user_data.acc = &acc;

  assert_int(0, ==,
             nghttp2_session_client_new(&session, &callbacks, &user_data));

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nvlen = ARRLEN(reqnv);
  nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);

  nghttp2_frame_headers_init(
    &frame->headers, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
    (int32_t)session->next_stream_id, NGHTTP2_HCAT_REQUEST, NULL, nva, nvlen);

  session->next_stream_id += 2;

  assert_int(0, ==, nghttp2_session_add_item(session, item));
  assert_not_null(nghttp2_outbound_queue_top(&session->ob_syn));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_HEADERS, ==, acc.buf[3]);
  assert_uint8((NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY), ==,
               acc.buf[4]);
  /* check stream id */
  assert_uint32(1, ==, nghttp2_get_uint32(&acc.buf[5]));

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_request_headers_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  int32_t stream_id = 1;
  nghttp2_nv malformed_nva[] = {MAKE_NV(":path", "\x01")};
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_priority_spec pri_spec;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_server_new(&session, &callbacks, &user_data);

  nghttp2_priority_spec_init(&pri_spec, 0, 255, 0);

  nghttp2_frame_headers_init(
    &frame.headers, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY, stream_id,
    NGHTTP2_HCAT_REQUEST, &pri_spec, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  assert_int(0, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.begin_headers_cb_called);
  stream = nghttp2_session_get_stream(session, stream_id);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* More than un-ACKed max concurrent streams leads REFUSED_STREAM */
  session->pending_local_max_concurrent_stream = 1;
  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             3, NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);
  session->local_settings.max_concurrent_streams =
    NGHTTP2_DEFAULT_MAX_CONCURRENT_STREAMS;

  /* Stream ID less than or equal to the previously received request
     HEADERS is just ignored due to race condition */
  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             3, NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);
  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* Stream ID is our side and it is idle stream ID, then treat it as
     connection error */
  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             2, NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_true(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  /* Check malformed headers. The library accept it. */
  nghttp2_session_server_new(&session, &callbacks, &user_data);

  nvlen = ARRLEN(malformed_nva);
  nghttp2_nv_array_copy(&nva, malformed_nva, nvlen, mem);
  nghttp2_frame_headers_init(&frame.headers,
                             NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
                             1, NGHTTP2_HCAT_HEADERS, NULL, nva, nvlen);
  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(0, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  /* Check client side */
  nghttp2_session_client_new(&session, &callbacks, &user_data);

  /* Receiving peer's idle stream ID is subject to connection error */
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_true(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  /* Receiving our's idle stream ID is subject to connection error */
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_true(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  session->next_stream_id = 5;
  session->last_sent_stream_id = 3;

  /* Stream ID which is not idle and not in stream map is just
     ignored */
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 3,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);
  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  nghttp2_session_server_new(&session, &callbacks, &user_data);

  /* Stream ID which is equal to local_last_stream_id is ok. */
  session->local_last_stream_id = 3;

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 3,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  assert_int(0, ==,
             nghttp2_session_on_request_headers_received(session, &frame));

  nghttp2_frame_headers_free(&frame.headers, mem);

  /* If GOAWAY has been sent, new stream is ignored */
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 5,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  session->goaway_flags |= NGHTTP2_GOAWAY_SENT;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);
  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);

  nghttp2_session_server_new(&session, &callbacks, &user_data);

  /* HEADERS to closed stream */
  stream = open_recv_stream(session, 1);
  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_RD);
  nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR);

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);

  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_response_headers_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  stream = open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  assert_int(
    0, ==,
    nghttp2_session_on_response_headers_received(session, &frame, stream));
  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);

  nghttp2_frame_headers_free(&frame.headers, mem);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_headers_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  stream = open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENED);
  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_WR);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  assert_int(0, ==,
             nghttp2_session_on_headers_received(session, &frame, stream));
  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);

  /* stream closed */
  frame.hd.flags |= NGHTTP2_FLAG_END_STREAM;

  assert_int(0, ==,
             nghttp2_session_on_headers_received(session, &frame, stream));
  assert_int(2, ==, user_data.begin_headers_cb_called);

  /* Check to see when NGHTTP2_STREAM_CLOSING, incoming HEADERS is
     discarded. */
  stream = open_sent_stream2(session, 3, NGHTTP2_STREAM_CLOSING);
  frame.hd.stream_id = 3;
  frame.hd.flags = NGHTTP2_FLAG_END_HEADERS;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_headers_received(session, &frame, stream));
  /* See no counters are updated */
  assert_int(2, ==, user_data.begin_headers_cb_called);
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);

  /* Server initiated stream */
  stream = open_recv_stream(session, 2);

  frame.hd.flags = NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM;
  frame.hd.stream_id = 2;

  assert_int(0, ==,
             nghttp2_session_on_headers_received(session, &frame, stream));
  assert_int(3, ==, user_data.begin_headers_cb_called);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);

  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_RD);

  /* Further reception of HEADERS is subject to stream error */
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_headers_received(session, &frame, stream));
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_push_response_headers_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  stream = open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  /* nghttp2_session_on_push_response_headers_received assumes
     stream's state is NGHTTP2_STREAM_RESERVED and session->server is
     0. */

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  assert_size(1, ==, session->num_incoming_reserved_streams);
  assert_int(
    0, ==,
    nghttp2_session_on_push_response_headers_received(session, &frame, stream));
  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_size(0, ==, session->num_incoming_reserved_streams);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);
  assert_size(1, ==, session->num_incoming_streams);
  assert_false(stream->flags & NGHTTP2_STREAM_FLAG_PUSH);

  /* If un-ACKed max concurrent streams limit is exceeded,
     RST_STREAMed */
  session->pending_local_max_concurrent_stream = 1;
  stream = open_recv_stream2(session, 4, NGHTTP2_STREAM_RESERVED);
  frame.hd.stream_id = 4;
  assert_int(
    NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
    nghttp2_session_on_push_response_headers_received(session, &frame, stream));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_REFUSED_STREAM, ==, item->frame.rst_stream.error_code);
  assert_size(1, ==, session->num_incoming_streams);
  assert_size(1, ==, session->num_incoming_reserved_streams);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(1, ==, session->num_incoming_streams);

  /* If ACKed max concurrent streams limit is exceeded, GOAWAY is
     issued */
  session->local_settings.max_concurrent_streams = 1;

  stream = open_recv_stream2(session, 6, NGHTTP2_STREAM_RESERVED);
  frame.hd.stream_id = 6;

  assert_int(
    NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
    nghttp2_session_on_push_response_headers_received(session, &frame, stream));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);
  assert_size(1, ==, session->num_incoming_streams);
  assert_size(1, ==, session->num_incoming_reserved_streams);

  nghttp2_frame_headers_free(&frame.headers, mem);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_rst_stream_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  nghttp2_session_server_new(&session, &callbacks, &user_data);
  open_recv_stream(session, 1);

  nghttp2_frame_rst_stream_init(&frame.rst_stream, 1, NGHTTP2_PROTOCOL_ERROR);

  assert_int(0, ==, nghttp2_session_on_rst_stream_received(session, &frame));
  assert_null(nghttp2_session_get_stream(session, 1));

  nghttp2_frame_rst_stream_free(&frame.rst_stream);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_settings_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_stream *stream1, *stream2;
  nghttp2_frame frame;
  const size_t niv = 5;
  nghttp2_settings_entry iv[255];
  nghttp2_outbound_item *item;
  nghttp2_nv nv = MAKE_NV(":authority", "example.org");
  nghttp2_mem *mem;
  nghttp2_option *option;
  uint8_t data[2048];
  nghttp2_frame_hd hd;
  int rv;
  nghttp2_ssize nread;
  nghttp2_stream *stream;

  mem = nghttp2_mem_default();

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 50;

  iv[1].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[1].value = 1000000009;

  iv[2].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[2].value = 64 * 1024;

  iv[3].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[3].value = 1024;

  iv[4].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
  iv[4].value = 0;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  session->remote_settings.initial_window_size = 16 * 1024;

  stream1 = open_sent_stream(session, 1);
  stream2 = open_recv_stream(session, 2);

  /* Set window size for each streams and will see how settings
     updates these values */
  stream1->remote_window_size = 16 * 1024;
  stream2->remote_window_size = -48 * 1024;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE,
                              dup_iv(iv, niv), niv);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));
  assert_uint32(1000000009, ==,
                session->remote_settings.max_concurrent_streams);
  assert_uint32(64 * 1024, ==, session->remote_settings.initial_window_size);
  assert_uint32(1024, ==, session->remote_settings.header_table_size);
  assert_uint32(0, ==, session->remote_settings.enable_push);

  assert_int32(64 * 1024, ==, stream1->remote_window_size);
  assert_int32(0, ==, stream2->remote_window_size);

  frame.settings.iv[2].value = 16 * 1024;

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  assert_int32(16 * 1024, ==, stream1->remote_window_size);
  assert_int32(-48 * 1024, ==, stream2->remote_window_size);

  assert_int32(
    16 * 1024, ==,
    nghttp2_session_get_stream_remote_window_size(session, stream1->stream_id));
  assert_int32(
    0, ==,
    nghttp2_session_get_stream_remote_window_size(session, stream2->stream_id));

  nghttp2_frame_settings_free(&frame.settings, mem);

  nghttp2_session_del(session);

  /* Check ACK with niv > 0 */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_ACK, dup_iv(iv, 1),
                              1);
  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));
  item = nghttp2_session_get_next_ob_item(session);
  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_frame_settings_free(&frame.settings, mem);
  nghttp2_session_del(session);

  /* Check ACK against no inflight SETTINGS */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_ACK, NULL, 0);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));
  item = nghttp2_session_get_next_ob_item(session);
  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_frame_settings_free(&frame.settings, mem);
  nghttp2_session_del(session);

  /* Check that 2 SETTINGS_HEADER_TABLE_SIZE 0 and 4096 are included
     and header table size is once cleared to 0. */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_submit_request2(session, NULL, &nv, 1, NULL, NULL);

  nghttp2_session_send(session);

  assert_size(0, <, session->hd_deflater.ctx.hd_table.len);

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 0;

  iv[1].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[1].value = 2048;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 2),
                              2);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  assert_size(0, ==, session->hd_deflater.ctx.hd_table.len);
  assert_size(2048, ==, session->hd_deflater.ctx.hd_table_bufsize_max);
  assert_uint32(2048, ==, session->remote_settings.header_table_size);

  nghttp2_frame_settings_free(&frame.settings, mem);
  nghttp2_session_del(session);

  /* Check that remote SETTINGS_MAX_CONCURRENT_STREAMS is set to a value set by
     nghttp2_option_set_peer_max_concurrent_streams() and reset to the default
     value (unlimited) after receiving initial SETTINGS frame from the peer. */
  nghttp2_option_new(&option);
  nghttp2_option_set_peer_max_concurrent_streams(option, 1000);
  nghttp2_session_client_new2(&session, &callbacks, NULL, option);
  assert_uint32(1000, ==, session->remote_settings.max_concurrent_streams);

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, NULL, 0);
  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));
  assert_uint32(NGHTTP2_DEFAULT_MAX_CONCURRENT_STREAMS, ==,
                session->remote_settings.max_concurrent_streams);

  nghttp2_frame_settings_free(&frame.settings, mem);
  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* Check too large SETTINGS_MAX_FRAME_SIZE */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
  iv[0].value = NGHTTP2_MAX_FRAME_SIZE_MAX + 1;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 1),
                              1);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_frame_settings_free(&frame.settings, mem);
  nghttp2_session_del(session);

  /* Check the case where stream window size overflows */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  stream1 = open_recv_stream(session, 1);

  /* This will increment window size by 1 */
  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 1,
                                   1);

  assert_int(0, ==, nghttp2_session_on_window_update_received(session, &frame));

  nghttp2_frame_window_update_free(&frame.window_update);

  iv[0].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[0].value = NGHTTP2_MAX_WINDOW_SIZE;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 1),
                              1);

  /* Now window size gets NGHTTP2_MAX_WINDOW_SIZE + 1, which is
     unacceptable situation in protocol spec. */
  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  nghttp2_frame_settings_free(&frame.settings, mem);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_SETTINGS, ==, item->frame.hd.type);

  item = nghttp2_outbound_queue_top(&session->ob_reg);

  assert_not_null(item);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_CLOSING, ==, stream1->state);

  nghttp2_session_del(session);

  /* It is invalid that peer disables ENABLE_CONNECT_PROTOCOL once it
     has been enabled. */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  session->remote_settings.enable_connect_protocol = 1;

  iv[0].settings_id = NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL;
  iv[0].value = 0;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 1),
                              1);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  nghttp2_frame_settings_free(&frame.settings, mem);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);

  /* Should send WINDOW_UPDATE with no_auto_window_update option on if
     the initial window size is decreased and becomes smaller than or
     equal to the amount of data that has already received. */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_window_update(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  iv[0].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[0].value = 1024;

  rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1);

  assert_int(0, ==, rv);

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);

  stream = open_recv_stream(session, 1);

  memset(data, 0, sizeof(data));
  hd.length = 1024;
  hd.type = NGHTTP2_DATA;
  hd.flags = NGHTTP2_FLAG_NONE;
  hd.stream_id = 1;
  nghttp2_frame_pack_frame_hd(data, &hd);

  nread =
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + hd.length);

  assert_ptrdiff((nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + hd.length), ==, nread);

  rv = nghttp2_session_consume(session, 1, hd.length);

  assert_int(0, ==, rv);
  assert_int32((int32_t)hd.length, ==, stream->recv_window_size);
  assert_int32((int32_t)hd.length, ==, stream->consumed_size);

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_ACK, NULL, 0);

  rv = nghttp2_session_on_settings_received(session, &frame, 0);

  assert_int(0, ==, rv);
  assert_int32(1024, ==, stream->local_window_size);
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(0, ==, stream->consumed_size);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32((int32_t)hd.length, ==,
               item->frame.window_update.window_size_increment);

  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* It is invalid to change SETTINGS_NO_RFC7540_PRIORITIES in the
     following SETTINGS. */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  session->remote_settings.no_rfc7540_priorities = 1;

  iv[0].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[0].value = 0;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 1),
                              1);

  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  nghttp2_frame_settings_free(&frame.settings, mem);

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_push_promise_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream, *promised_stream;
  nghttp2_outbound_item *item;
  nghttp2_nv malformed_nva[] = {MAKE_NV(":path", "\x01")};
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_mem *mem;
  nghttp2_settings_entry iv = {NGHTTP2_SETTINGS_ENABLE_PUSH, 0};

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_begin_headers_callback = on_begin_headers_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  stream = open_sent_stream(session, 1);

  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 2, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  assert_int(0, ==, nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);
  promised_stream = nghttp2_session_get_stream(session, 2);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_RESERVED, ==,
              promised_stream->state);
  assert_int32(2, ==, session->last_recv_stream_id);

  /* Attempt to PUSH_PROMISE against half close (remote) */
  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_RD);
  frame.push_promise.promised_stream_id = 4;

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);
  assert_null(nghttp2_session_get_stream(session, 4));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_STREAM_CLOSED, ==, item->frame.goaway.error_code);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(4, ==, session->last_recv_stream_id);

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  stream = open_sent_stream(session, 1);

  /* Attempt to PUSH_PROMISE against stream in closing state */
  stream->state = NGHTTP2_STREAM_CLOSING;
  frame.push_promise.promised_stream_id = 6;

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_size(0, ==, session->num_incoming_reserved_streams);
  assert_null(nghttp2_session_get_stream(session, 6));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(6, ==, item->frame.hd.stream_id);
  assert_uint32(NGHTTP2_CANCEL, ==, item->frame.rst_stream.error_code);
  assert_int(0, ==, nghttp2_session_send(session));

  /* Attempt to PUSH_PROMISE against idle stream */
  frame.hd.stream_id = 3;
  frame.push_promise.promised_stream_id = 8;

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_size(0, ==, session->num_incoming_reserved_streams);
  assert_null(nghttp2_session_get_stream(session, 8));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.hd.stream_id);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);
  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  stream = open_sent_stream(session, 1);

  /* Same ID twice */
  frame.hd.stream_id = 1;
  frame.push_promise.promised_stream_id = 2;

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(0, ==, nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);
  assert_not_null(nghttp2_session_get_stream(session, 2));

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);
  assert_null(nghttp2_session_get_stream(session, 8));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);
  assert_int(0, ==, nghttp2_session_send(session));

  /* After GOAWAY, PUSH_PROMISE will be discarded */
  frame.push_promise.promised_stream_id = 10;

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);
  assert_null(nghttp2_session_get_stream(session, 10));
  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  /* Attempt to PUSH_PROMISE against reserved (remote) stream */
  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  2, 4, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_size(1, ==, session->num_incoming_reserved_streams);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);

  /* Disable PUSH */
  nghttp2_session_client_new(&session, &callbacks, &user_data);

  open_sent_stream(session, 1);

  session->local_settings.enable_push = 0;

  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 2, NULL, 0);

  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(0, ==, user_data.begin_headers_cb_called);
  assert_int(1, ==, user_data.invalid_frame_recv_cb_called);
  assert_size(0, ==, session->num_incoming_reserved_streams);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);

  /* Check malformed headers. We accept malformed headers */
  nghttp2_session_client_new(&session, &callbacks, &user_data);

  open_sent_stream(session, 1);

  nvlen = ARRLEN(malformed_nva);
  nghttp2_nv_array_copy(&nva, malformed_nva, nvlen, mem);
  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 2, nva, nvlen);
  user_data.begin_headers_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;
  assert_int(0, ==, nghttp2_session_on_push_promise_received(session, &frame));

  assert_int(1, ==, user_data.begin_headers_cb_called);
  assert_int(0, ==, user_data.invalid_frame_recv_cb_called);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);

  /* If local_settings.enable_push = 0 is pending, but not acked from
     peer, incoming PUSH_PROMISE is rejected */
  nghttp2_session_client_new(&session, &callbacks, &user_data);

  open_sent_stream(session, 1);

  /* Submit settings with ENABLE_PUSH = 0 (thus disabling push) */
  nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1);

  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 2, NULL, 0);

  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_size(0, ==, session->num_incoming_reserved_streams);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);

  /* Check max_incoming_reserved_streams */
  nghttp2_session_client_new(&session, &callbacks, &user_data);
  session->max_incoming_reserved_streams = 1;

  open_sent_stream(session, 1);
  open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  assert_size(1, ==, session->num_incoming_reserved_streams);

  nghttp2_frame_push_promise_init(&frame.push_promise, NGHTTP2_FLAG_END_HEADERS,
                                  1, 4, NULL, 0);

  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_push_promise_received(session, &frame));

  assert_size(1, ==, session->num_incoming_reserved_streams);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_CANCEL, ==, item->frame.rst_stream.error_code);

  nghttp2_frame_push_promise_free(&frame.push_promise, mem);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_ping_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_outbound_item *top;
  const uint8_t opaque_data[] = "01234567";
  nghttp2_option *option;

  user_data.frame_recv_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  nghttp2_frame_ping_init(&frame.ping, NGHTTP2_FLAG_ACK, opaque_data);

  assert_int(0, ==, nghttp2_session_on_ping_received(session, &frame));
  assert_int(1, ==, user_data.frame_recv_cb_called);

  /* Since this ping frame has ACK flag set, no further action is
     performed. */
  assert_null(nghttp2_outbound_queue_top(&session->ob_urgent));

  /* Clear the flag, and receive it again */
  frame.hd.flags = NGHTTP2_FLAG_NONE;

  assert_int(0, ==, nghttp2_session_on_ping_received(session, &frame));
  assert_int(2, ==, user_data.frame_recv_cb_called);
  top = nghttp2_outbound_queue_top(&session->ob_urgent);
  assert_uint8(NGHTTP2_PING, ==, top->frame.hd.type);
  assert_uint8(NGHTTP2_FLAG_ACK, ==, top->frame.hd.flags);
  assert_memory_equal(8, opaque_data, top->frame.ping.opaque_data);

  nghttp2_frame_ping_free(&frame.ping);
  nghttp2_session_del(session);

  /* Use nghttp2_option_set_no_auto_ping_ack() */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_ping_ack(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, &user_data, option);
  nghttp2_frame_ping_init(&frame.ping, NGHTTP2_FLAG_NONE, NULL);

  user_data.frame_recv_cb_called = 0;

  assert_int(0, ==, nghttp2_session_on_ping_received(session, &frame));
  assert_int(1, ==, user_data.frame_recv_cb_called);
  assert_null(nghttp2_outbound_queue_top(&session->ob_urgent));

  nghttp2_frame_ping_free(&frame.ping);
  nghttp2_session_del(session);
  nghttp2_option_del(option);
}

void test_nghttp2_session_on_goaway_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  int i;
  nghttp2_mem *mem;
  const uint8_t *data;
  nghttp2_ssize datalen;

  mem = nghttp2_mem_default();
  user_data.frame_recv_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;
  callbacks.on_stream_close_callback = on_stream_close_callback;

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  for (i = 1; i <= 7; ++i) {
    if (nghttp2_session_is_my_stream_id(session, i)) {
      open_sent_stream(session, i);
    } else {
      open_recv_stream(session, i);
    }
  }

  nghttp2_frame_goaway_init(&frame.goaway, 3, NGHTTP2_PROTOCOL_ERROR, NULL, 0);

  user_data.stream_close_cb_called = 0;

  assert_int(0, ==, nghttp2_session_on_goaway_received(session, &frame));

  assert_int(1, ==, user_data.frame_recv_cb_called);
  assert_int32(3, ==, session->remote_last_stream_id);
  /* on_stream_close should be callsed for 2 times (stream 5 and 7) */
  assert_int(2, ==, user_data.stream_close_cb_called);

  assert_not_null(nghttp2_session_get_stream(session, 1));
  assert_not_null(nghttp2_session_get_stream(session, 2));
  assert_not_null(nghttp2_session_get_stream(session, 3));
  assert_not_null(nghttp2_session_get_stream(session, 4));
  assert_null(nghttp2_session_get_stream(session, 5));
  assert_not_null(nghttp2_session_get_stream(session, 6));
  assert_null(nghttp2_session_get_stream(session, 7));

  nghttp2_frame_goaway_free(&frame.goaway, mem);
  nghttp2_session_del(session);

  /* Make sure that no memory leak when stream_close callback fails
     with a fatal error */
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_stream_close_callback = fatal_error_on_stream_close_callback;

  memset(&user_data, 0, sizeof(user_data));

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  nghttp2_frame_goaway_init(&frame.goaway, 0, NGHTTP2_NO_ERROR, NULL, 0);

  assert_int(0, ==, nghttp2_session_on_goaway_received(session, &frame));

  nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  datalen = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(NGHTTP2_ERR_CALLBACK_FAILURE, ==, datalen);
  assert_int(1, ==, user_data.stream_close_cb_called);

  nghttp2_frame_goaway_free(&frame.goaway, mem);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_window_update_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  nghttp2_outbound_item *data_item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;
  user_data.frame_recv_cb_called = 0;
  user_data.invalid_frame_recv_cb_called = 0;

  nghttp2_session_client_new(&session, &callbacks, &user_data);

  stream = open_sent_stream(session, 1);

  data_item = create_data_ob_item(mem);

  nghttp2_stream_attach_item(stream, data_item);

  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 1,
                                   16 * 1024);

  assert_int(0, ==, nghttp2_session_on_window_update_received(session, &frame));
  assert_int(1, ==, user_data.frame_recv_cb_called);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 16 * 1024, ==,
               stream->remote_window_size);

  nghttp2_stream_defer_item(stream, NGHTTP2_STREAM_FLAG_DEFERRED_FLOW_CONTROL);

  assert_int(0, ==, nghttp2_session_on_window_update_received(session, &frame));
  assert_int(2, ==, user_data.frame_recv_cb_called);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 16 * 1024 * 2, ==,
               stream->remote_window_size);
  assert_false(stream->flags & NGHTTP2_STREAM_FLAG_DEFERRED_ALL);

  nghttp2_frame_window_update_free(&frame.window_update);

  /* Receiving WINDOW_UPDATE on reserved (remote) stream is a
     connection error */
  open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 2,
                                   4096);

  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);
  assert_int(0, ==, nghttp2_session_on_window_update_received(session, &frame));
  assert_true(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  nghttp2_frame_window_update_free(&frame.window_update);

  nghttp2_session_del(session);

  /* Receiving WINDOW_UPDATE on reserved (local) stream is allowed */
  nghttp2_session_server_new(&session, &callbacks, &user_data);

  stream = open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 2,
                                   4096);

  assert_int(0, ==, nghttp2_session_on_window_update_received(session, &frame));
  assert_false(session->goaway_flags & NGHTTP2_GOAWAY_TERM_ON_SEND);

  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 4096, ==,
               stream->remote_window_size);

  nghttp2_frame_window_update_free(&frame.window_update);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_data_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_outbound_item *top;
  nghttp2_stream *stream;
  nghttp2_frame frame;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  stream = open_recv_stream(session, 2);

  nghttp2_frame_hd_init(&frame.hd, 4096, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 2);

  assert_int(0, ==, nghttp2_session_on_data_received(session, &frame));
  assert_uint8(0, ==, stream->shut_flags);

  frame.hd.flags = NGHTTP2_FLAG_END_STREAM;

  assert_int(0, ==, nghttp2_session_on_data_received(session, &frame));
  assert_uint8(NGHTTP2_SHUT_RD, ==, stream->shut_flags);

  /* If NGHTTP2_STREAM_CLOSING state, DATA frame is discarded. */
  open_sent_stream2(session, 1, NGHTTP2_STREAM_CLOSING);

  frame.hd.flags = NGHTTP2_FLAG_NONE;
  frame.hd.stream_id = 1;

  assert_int(0, ==, nghttp2_session_on_data_received(session, &frame));
  assert_null(nghttp2_outbound_queue_top(&session->ob_reg));

  /* Check INVALID_STREAM case: DATA frame with stream ID which does
     not exist. */

  frame.hd.stream_id = 3;

  assert_int(0, ==, nghttp2_session_on_data_received(session, &frame));
  top = nghttp2_outbound_queue_top(&session->ob_reg);
  /* DATA against nonexistent stream is just ignored for now. */
  assert_null(top);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_data_received_fail_fast(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  uint8_t buf[9];
  nghttp2_stream *stream;
  nghttp2_frame_hd hd;
  nghttp2_outbound_item *item;

  memset(&callbacks, 0, sizeof(callbacks));

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 1);
  nghttp2_frame_pack_frame_hd(buf, &hd);

  nghttp2_session_server_new(&session, &callbacks, NULL);

  /* DATA to closed (remote) */
  stream = open_recv_stream(session, 1);
  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_RD);

  assert_ptrdiff((nghttp2_ssize)sizeof(buf), ==,
                 nghttp2_session_mem_recv2(session, buf, sizeof(buf)));

  item = nghttp2_session_get_next_ob_item(session);

  assert_not_null(item);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_session_del(session);

  nghttp2_session_server_new(&session, &callbacks, NULL);

  /* DATA to closed stream with explicit closed (remote) */
  stream = open_recv_stream(session, 1);
  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_RD);
  nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR);

  assert_ptrdiff((nghttp2_ssize)sizeof(buf), ==,
                 nghttp2_session_mem_recv2(session, buf, sizeof(buf)));

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_altsvc_received(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_frame frame;
  nghttp2_option *option;
  uint8_t origin[] = "nghttp2.org";
  uint8_t field_value[] = "h2=\":443\"";
  int rv;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_builtin_recv_extension_type(option, NGHTTP2_ALTSVC);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  frame.ext.payload = &session->iframe.ext_frame_payload;

  /* We just pass the strings without making a copy.  This is OK,
     since we never call nghttp2_frame_altsvc_free(). */
  nghttp2_frame_altsvc_init(&frame.ext, 0, origin, sizeof(origin) - 1,
                            field_value, sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_on_altsvc_received(session, &frame);

  assert_int(0, ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* Receiving empty origin with stream ID == 0 */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  frame.ext.payload = &session->iframe.ext_frame_payload;

  nghttp2_frame_altsvc_init(&frame.ext, 0, origin, 0, field_value,
                            sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_on_altsvc_received(session, &frame);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* Receiving non-empty origin with stream ID != 0 */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  frame.ext.payload = &session->iframe.ext_frame_payload;

  open_sent_stream(session, 1);

  nghttp2_frame_altsvc_init(&frame.ext, 1, origin, sizeof(origin) - 1,
                            field_value, sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_on_altsvc_received(session, &frame);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* Receiving empty origin with stream ID != 0; this is OK */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  frame.ext.payload = &session->iframe.ext_frame_payload;

  open_sent_stream(session, 1);

  nghttp2_frame_altsvc_init(&frame.ext, 1, origin, 0, field_value,
                            sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_on_altsvc_received(session, &frame);

  assert_int(0, ==, rv);
  assert_int(1, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  /* Stream does not exist; ALTSVC will be ignored. */
  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  frame.ext.payload = &session->iframe.ext_frame_payload;

  nghttp2_frame_altsvc_init(&frame.ext, 1, origin, 0, field_value,
                            sizeof(field_value) - 1);

  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_on_altsvc_received(session, &frame);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_recv_cb_called);

  nghttp2_session_del(session);

  nghttp2_option_del(option);
}

void test_nghttp2_session_send_headers_start_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_headers_init(&frame->headers, NGHTTP2_FLAG_END_HEADERS,
                             (int32_t)session->next_stream_id,
                             NGHTTP2_HCAT_REQUEST, NULL, NULL, 0);
  session->next_stream_id += 2;

  nghttp2_session_add_item(session, item);
  assert_int(0, ==, nghttp2_session_send(session));
  stream = nghttp2_session_get_stream(session, 1);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);

  nghttp2_session_del(session);
}

void test_nghttp2_session_send_headers_reply(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, NULL));
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_headers_init(&frame->headers, NGHTTP2_FLAG_END_HEADERS, 1,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  nghttp2_session_add_item(session, item);
  assert_int(0, ==, nghttp2_session_send(session));
  stream = nghttp2_session_get_stream(session, 1);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);

  nghttp2_session_del(session);
}

void test_nghttp2_session_send_headers_frame_size_error(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_nv *nva;
  size_t nvlen;
  size_t vallen = NGHTTP2_HD_MAX_NV;
  nghttp2_nv nv[28];
  size_t nnv = ARRLEN(nv);
  size_t i;
  my_user_data ud;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  for (i = 0; i < nnv; ++i) {
    nv[i].name = (uint8_t *)"header";
    nv[i].namelen = strlen((const char *)nv[i].name);
    nv[i].value = mem->malloc(vallen + 1, NULL);
    memset(nv[i].value, '0' + (int)i, vallen);
    nv[i].value[vallen] = '\0';
    nv[i].valuelen = vallen;
    nv[i].flags = NGHTTP2_NV_FLAG_NONE;
  }

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);
  nvlen = nnv;
  nghttp2_nv_array_copy(&nva, nv, nvlen, mem);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_headers_init(&frame->headers, NGHTTP2_FLAG_END_HEADERS,
                             (int32_t)session->next_stream_id,
                             NGHTTP2_HCAT_REQUEST, NULL, nva, nvlen);

  session->next_stream_id += 2;

  nghttp2_session_add_item(session, item);

  ud.frame_not_send_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_FRAME_SIZE_ERROR, ==, ud.not_sent_error);

  for (i = 0; i < nnv; ++i) {
    mem->free(nv[i].value, NULL);
  }
  nghttp2_session_del(session);
}

void test_nghttp2_session_send_headers_push_reply(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, NULL));
  open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_headers_init(&frame->headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  nghttp2_session_add_item(session, item);
  assert_size(0, ==, session->num_outgoing_streams);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(1, ==, session->num_outgoing_streams);
  stream = nghttp2_session_get_stream(session, 2);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);
  assert_false(stream->flags & NGHTTP2_STREAM_FLAG_PUSH);
  nghttp2_session_del(session);
}

void test_nghttp2_session_send_rst_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  nghttp2_session_client_new(&session, &callbacks, &user_data);
  open_sent_stream(session, 1);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_rst_stream_init(&frame->rst_stream, 1, NGHTTP2_PROTOCOL_ERROR);
  nghttp2_session_add_item(session, item);
  assert_int(0, ==, nghttp2_session_send(session));

  assert_null(nghttp2_session_get_stream(session, 1));

  nghttp2_session_del(session);
}

void test_nghttp2_session_send_push_promise(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_stream *stream;
  nghttp2_settings_entry iv;
  my_user_data ud;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);
  open_recv_stream(session, 1);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_push_promise_init(&frame->push_promise,
                                  NGHTTP2_FLAG_END_HEADERS, 1,
                                  (int32_t)session->next_stream_id, NULL, 0);

  session->next_stream_id += 2;

  nghttp2_session_add_item(session, item);

  assert_int(0, ==, nghttp2_session_send(session));
  stream = nghttp2_session_get_stream(session, 2);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_RESERVED, ==, stream->state);

  /* Received ENABLE_PUSH = 0 */
  iv.settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
  iv.value = 0;
  frame = mem->malloc(sizeof(nghttp2_frame), NULL);
  nghttp2_frame_settings_init(&frame->settings, NGHTTP2_FLAG_NONE,
                              dup_iv(&iv, 1), 1);
  nghttp2_session_on_settings_received(session, frame, 1);
  nghttp2_frame_settings_free(&frame->settings, mem);
  mem->free(frame, NULL);

  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_push_promise_init(&frame->push_promise,
                                  NGHTTP2_FLAG_END_HEADERS, 1, -1, NULL, 0);
  nghttp2_session_add_item(session, item);

  ud.frame_not_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_PUSH_PROMISE, ==, ud.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_PUSH_DISABLED, ==, ud.not_sent_error);

  nghttp2_session_del(session);

  /* PUSH_PROMISE from client is error */
  nghttp2_session_client_new(&session, &callbacks, &ud);
  open_sent_stream(session, 1);
  item = mem->malloc(sizeof(nghttp2_outbound_item), NULL);

  nghttp2_outbound_item_init(item);

  frame = &item->frame;

  nghttp2_frame_push_promise_init(&frame->push_promise,
                                  NGHTTP2_FLAG_END_HEADERS, 1, -1, NULL, 0);
  nghttp2_session_add_item(session, item);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_null(nghttp2_session_get_stream(session, 3));

  nghttp2_session_del(session);
}

void test_nghttp2_session_is_my_stream_id(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  nghttp2_session_server_new(&session, &callbacks, NULL);

  assert_false(nghttp2_session_is_my_stream_id(session, 0));
  assert_false(nghttp2_session_is_my_stream_id(session, 1));
  assert_true(nghttp2_session_is_my_stream_id(session, 2));

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, NULL);

  assert_false(nghttp2_session_is_my_stream_id(session, 0));
  assert_true(nghttp2_session_is_my_stream_id(session, 1));
  assert_false(nghttp2_session_is_my_stream_id(session, 2));

  nghttp2_session_del(session);
}

void test_nghttp2_session_upgrade2(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  uint8_t settings_payload[128];
  size_t settings_payloadlen;
  nghttp2_settings_entry iv[16];
  nghttp2_stream *stream;
  nghttp2_outbound_item *item;
  nghttp2_ssize rv;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 1;
  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 4095;
  settings_payloadlen = (size_t)nghttp2_pack_settings_payload2(
    settings_payload, sizeof(settings_payload), iv, 2);

  /* Check client side */
  nghttp2_session_client_new(&session, &callbacks, NULL);
  assert_int(0, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, &callbacks));
  assert_int32(1, ==, session->last_sent_stream_id);
  stream = nghttp2_session_get_stream(session, 1);
  assert_not_null(stream);
  assert_ptr_equal(&callbacks, stream->stream_user_data);
  assert_uint8(NGHTTP2_SHUT_WR, ==, stream->shut_flags);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_SETTINGS, ==, item->frame.hd.type);
  assert_size(2, ==, item->frame.settings.niv);
  assert_int32(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, ==,
               item->frame.settings.iv[0].settings_id);
  assert_uint32(1, ==, item->frame.settings.iv[0].value);
  assert_int32(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, ==,
               item->frame.settings.iv[1].settings_id);
  assert_uint32(4095, ==, item->frame.settings.iv[1].value);

  /* Call nghttp2_session_upgrade2() again is error */
  assert_int(NGHTTP2_ERR_PROTO, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, &callbacks));
  nghttp2_session_del(session);

  /* Make sure that response from server can be received */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  assert_int(0, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, &callbacks));

  stream = nghttp2_session_get_stream(session, 1);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);

  nghttp2_hd_deflate_init(&deflater, mem);
  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, resnv,
                    ARRLEN(resnv), mem);

  assert_ptrdiff(0, ==, rv);

  buf = &bufs.head->buf;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_bufs_reset(&bufs);

  /* Check server side */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  assert_int(0, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, &callbacks));
  assert_int32(1, ==, session->last_recv_stream_id);
  stream = nghttp2_session_get_stream(session, 1);
  assert_not_null(stream);
  assert_null(stream->stream_user_data);
  assert_uint8(NGHTTP2_SHUT_RD, ==, stream->shut_flags);
  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_uint32(1, ==, session->remote_settings.max_concurrent_streams);
  assert_uint32(4095, ==, session->remote_settings.initial_window_size);
  /* Call nghttp2_session_upgrade2() again is error */
  assert_int(NGHTTP2_ERR_PROTO, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, &callbacks));
  nghttp2_session_del(session);

  /* Empty SETTINGS is OK */
  settings_payloadlen = (size_t)nghttp2_pack_settings_payload2(
    settings_payload, sizeof(settings_payload), NULL, 0);

  nghttp2_session_client_new(&session, &callbacks, NULL);
  assert_int(0, ==,
             nghttp2_session_upgrade2(session, settings_payload,
                                      settings_payloadlen, 0, NULL));
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_submit_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_frame *frame;
  nghttp2_frame_hd hd;
  nghttp2_active_outbound_item *aob;
  nghttp2_bufs *framebufs;
  nghttp2_buf *buf;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = block_count_send_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 2;
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  aob = &session->aob;
  framebufs = &aob->framebufs;

  open_sent_stream(session, 1);

  assert_int(
    0, ==,
    nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd));

  ud.block_count = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  frame = &aob->item->frame;

  buf = &framebufs->head->buf;
  nghttp2_frame_unpack_frame_hd(&hd, buf->pos);

  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, frame->hd.flags);
  /* aux_data.data.flags has these flags */
  assert_uint8(NGHTTP2_FLAG_END_STREAM, ==, aob->item->aux_data.data.flags);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_data_read_length_too_large(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_frame *frame;
  nghttp2_frame_hd hd;
  nghttp2_active_outbound_item *aob;
  nghttp2_bufs *framebufs;
  nghttp2_buf *buf;
  size_t payloadlen;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = block_count_send_callback;
  callbacks.read_length_callback2 = too_large_data_source_length_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 2;
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  aob = &session->aob;
  framebufs = &aob->framebufs;

  open_sent_stream(session, 1);

  assert_int(
    0, ==,
    nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd));

  ud.block_count = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  frame = &aob->item->frame;

  buf = &framebufs->head->buf;
  nghttp2_frame_unpack_frame_hd(&hd, buf->pos);

  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, frame->hd.flags);
  assert_size(16384, ==, hd.length);
  /* aux_data.data.flags has these flags */
  assert_uint8(NGHTTP2_FLAG_END_STREAM, ==, aob->item->aux_data.data.flags);

  nghttp2_session_del(session);

  /* Check that buffers are expanded */
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));

  ud.data_source_length = NGHTTP2_MAX_FRAME_SIZE_MAX;

  session->remote_settings.max_frame_size = NGHTTP2_MAX_FRAME_SIZE_MAX;

  open_sent_stream(session, 1);

  assert_int(
    0, ==,
    nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd));

  ud.block_count = 0;
  assert_int(0, ==, nghttp2_session_send(session));

  aob = &session->aob;

  frame = &aob->item->frame;

  framebufs = &aob->framebufs;

  buf = &framebufs->head->buf;
  nghttp2_frame_unpack_frame_hd(&hd, buf->pos);

  payloadlen = nghttp2_min_size(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE,
                                NGHTTP2_INITIAL_WINDOW_SIZE);

  assert_size(NGHTTP2_FRAME_HDLEN + 1 + payloadlen, ==,
              (size_t)nghttp2_buf_cap(buf));
  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, frame->hd.flags);
  assert_size(payloadlen, ==, hd.length);
  /* aux_data.data.flags has these flags */
  assert_uint8(NGHTTP2_FLAG_END_STREAM, ==, aob->item->aux_data.data.flags);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_data_read_length_smallest(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_frame *frame;
  nghttp2_frame_hd hd;
  nghttp2_active_outbound_item *aob;
  nghttp2_bufs *framebufs;
  nghttp2_buf *buf;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = block_count_send_callback;
  callbacks.read_length_callback2 = smallest_length_data_source_length_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 2;
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  aob = &session->aob;
  framebufs = &aob->framebufs;

  open_sent_stream(session, 1);

  assert_int(
    0, ==,
    nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd));

  ud.block_count = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  frame = &aob->item->frame;

  buf = &framebufs->head->buf;
  nghttp2_frame_unpack_frame_hd(&hd, buf->pos);

  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, frame->hd.flags);
  assert_size(1, ==, hd.length);
  /* aux_data.data.flags has these flags */
  assert_uint8(NGHTTP2_FLAG_END_STREAM, ==, aob->item->aux_data.data.flags);

  nghttp2_session_del(session);
}

static nghttp2_ssize submit_data_twice_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  (void)session;
  (void)stream_id;
  (void)buf;
  (void)source;
  (void)user_data;

  *data_flags |= NGHTTP2_DATA_FLAG_EOF;
  return (nghttp2_ssize)nghttp2_min_size(len, 16);
}

static int submit_data_twice_on_frame_send_callback(nghttp2_session *session,
                                                    const nghttp2_frame *frame,
                                                    void *user_data) {
  static int called = 0;
  int rv;
  nghttp2_data_provider2 data_prd;
  (void)user_data;

  if (called == 0) {
    called = 1;

    data_prd.read_callback = submit_data_twice_data_source_read_callback;

    rv = nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM,
                              frame->hd.stream_id, &data_prd);
    assert_int(0, ==, rv);
  }

  return 0;
}

void test_nghttp2_submit_data_twice(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  accumulator acc;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  callbacks.on_frame_send_callback = submit_data_twice_on_frame_send_callback;

  data_prd.read_callback = submit_data_twice_data_source_read_callback;

  acc.length = 0;
  ud.acc = &acc;

  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));

  open_sent_stream(session, 1);

  assert_int(0, ==,
             nghttp2_submit_data2(session, NGHTTP2_FLAG_NONE, 1, &data_prd));
  assert_int(0, ==, nghttp2_session_send(session));

  /* We should have sent 2 DATA frame with 16 bytes payload each */
  assert_size(NGHTTP2_FRAME_HDLEN * 2 + 16 * 2, ==, acc.length);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_request_with_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = 64 * 1024 - 1;
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  assert_int32(1, ==,
               nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv),
                                       &data_prd, NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(reqnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(reqnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(0, ==, ud.data_source_length);

  nghttp2_session_del(session);

  /* nghttp2_submit_request2() with server session is error */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  assert_int32(
    NGHTTP2_ERR_PROTO, ==,
    nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_request_without_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  accumulator acc;
  nghttp2_data_provider2 data_prd = {{-1}, NULL};
  nghttp2_outbound_item *item;
  my_user_data ud;
  nghttp2_frame frame;
  nghttp2_hd_inflater inflater;
  nva_out out;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nva_out_init(&out);
  acc.length = 0;
  ud.acc = &acc;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));

  nghttp2_hd_inflate_init(&inflater, mem);
  assert_int32(1, ==,
               nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv),
                                       &data_prd, NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(reqnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(reqnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_true(item->frame.hd.flags & NGHTTP2_FLAG_END_STREAM);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, unpack_frame(&frame, acc.buf, acc.length));

  nghttp2_bufs_add(&bufs, acc.buf, acc.length);
  inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem);

  assert_size(ARRLEN(reqnv), ==, out.nvlen);
  assert_nv_equal(reqnv, out.nva, out.nvlen, mem);
  nghttp2_frame_headers_free(&frame.headers, mem);
  nva_out_reset(&out, mem);

  nghttp2_bufs_free(&bufs);
  nghttp2_hd_inflate_free(&inflater);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_response_with_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = 64 * 1024 - 1;
  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  assert_int(
    0, ==,
    nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), &data_prd));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(resnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(resnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(0, ==, ud.data_source_length);

  nghttp2_session_del(session);

  /* Various error cases */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  /* Calling nghttp2_submit_response2() with client session is error */
  assert_int(NGHTTP2_ERR_PROTO, ==,
             nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), NULL));

  /* Stream ID <= 0 is error */
  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_response2(session, 0, resnv, ARRLEN(resnv), NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_response_without_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  accumulator acc;
  nghttp2_data_provider2 data_prd = {{-1}, NULL};
  nghttp2_outbound_item *item;
  my_user_data ud;
  nghttp2_frame frame;
  nghttp2_hd_inflater inflater;
  nva_out out;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nva_out_init(&out);
  acc.length = 0;
  ud.acc = &acc;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));

  nghttp2_hd_inflate_init(&inflater, mem);
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  assert_int(
    0, ==,
    nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), &data_prd));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(resnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(resnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_true(item->frame.hd.flags & NGHTTP2_FLAG_END_STREAM);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, unpack_frame(&frame, acc.buf, acc.length));

  nghttp2_bufs_add(&bufs, acc.buf, acc.length);
  inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem);

  assert_size(ARRLEN(resnv), ==, out.nvlen);
  assert_nv_equal(resnv, out.nva, out.nvlen, mem);

  nva_out_reset(&out, mem);
  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame.headers, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_session_del(session);
}

void test_nghttp2_submit_response_push_response(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  session->goaway_flags |= NGHTTP2_GOAWAY_RECV;

  assert_int(0, ==,
             nghttp2_submit_response2(session, 2, resnv, ARRLEN(resnv), NULL));

  ud.frame_not_send_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_not_send_cb_called);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_trailer(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  accumulator acc;
  nghttp2_data_provider2 data_prd;
  nghttp2_outbound_item *item;
  my_user_data ud;
  nghttp2_frame frame;
  nghttp2_hd_inflater inflater;
  nva_out out;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  data_prd.read_callback = no_end_stream_data_source_read_callback;
  nva_out_init(&out);
  acc.length = 0;
  ud.acc = &acc;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));

  nghttp2_hd_inflate_init(&inflater, mem);
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  assert_int(
    0, ==,
    nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), &data_prd));
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(0, ==,
             nghttp2_submit_trailer(session, 1, trailernv, ARRLEN(trailernv)));

  session->callbacks.send_callback2 = accumulator_send_callback;

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_enum(nghttp2_headers_category, NGHTTP2_HCAT_HEADERS, ==,
              item->frame.headers.cat);
  assert_true(item->frame.hd.flags & NGHTTP2_FLAG_END_STREAM);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, unpack_frame(&frame, acc.buf, acc.length));

  nghttp2_bufs_add(&bufs, acc.buf, acc.length);
  inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem);

  assert_size(ARRLEN(trailernv), ==, out.nvlen);
  assert_nv_equal(trailernv, out.nva, out.nvlen, mem);

  nva_out_reset(&out, mem);
  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame.headers, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_session_del(session);

  /* Specifying stream ID <= 0 is error */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  open_recv_stream(session, 1);

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_trailer(session, 0, trailernv, ARRLEN(trailernv)));

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_trailer(session, -1, trailernv, ARRLEN(trailernv)));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers_start_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, NULL));
  assert_int32(1, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, -1,
                                      NULL, reqnv, ARRLEN(reqnv), NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(reqnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(reqnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_uint8((NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM), ==,
               item->frame.hd.flags);
  assert_false(item->frame.hd.flags & NGHTTP2_FLAG_PRIORITY);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers_reply(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      resnv, ARRLEN(resnv), NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(resnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(resnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_uint8((NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS), ==,
               item->frame.hd.flags);

  ud.frame_send_cb_called = 0;
  ud.sent_frame_type = 0;
  /* The transimission will be canceled because the stream 1 is not
     open. */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, ud.frame_send_cb_called);

  stream = open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      resnv, ARRLEN(resnv), NULL));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.sent_frame_type);
  assert_true(stream->shut_flags & NGHTTP2_SHUT_WR);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers_push_reply(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_stream *stream;
  int foo;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));
  stream = open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, 2, NULL,
                                      resnv, ARRLEN(resnv), &foo));

  ud.frame_send_cb_called = 0;
  ud.sent_frame_type = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.sent_frame_type);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);
  assert_ptr_equal(&foo, stream->stream_user_data);

  nghttp2_session_del(session);

  /* Sending HEADERS from client against stream in reserved state is
     error */
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, 2, NULL,
                                      reqnv, ARRLEN(reqnv), NULL));

  ud.frame_send_cb_called = 0;
  ud.sent_frame_type = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, ud.frame_send_cb_called);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;
  accumulator acc;
  nghttp2_frame frame;
  nghttp2_hd_inflater inflater;
  nva_out out;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nva_out_init(&out);
  acc.length = 0;
  ud.acc = &acc;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));

  nghttp2_hd_inflate_init(&inflater, mem);
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      reqnv, ARRLEN(reqnv), NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(reqnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(reqnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);
  assert_uint8((NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS), ==,
               item->frame.hd.flags);

  ud.frame_send_cb_called = 0;
  ud.sent_frame_type = 0;
  /* The transimission will be canceled because the stream 1 is not
     open. */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, ud.frame_send_cb_called);

  stream = open_sent_stream(session, 1);

  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      reqnv, ARRLEN(reqnv), NULL));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.sent_frame_type);
  assert_true(stream->shut_flags & NGHTTP2_SHUT_WR);

  assert_int(0, ==, unpack_frame(&frame, acc.buf, acc.length));

  nghttp2_bufs_add(&bufs, acc.buf, acc.length);
  inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem);

  assert_size(ARRLEN(reqnv), ==, out.nvlen);
  assert_nv_equal(reqnv, out.nva, out.nvlen, mem);

  nva_out_reset(&out, mem);
  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_hd_inflate_free(&inflater);

  nghttp2_session_del(session);

  /* Error cases with invalid stream ID */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  /* Sending nghttp2_submit_headers() with stream_id == 1 and server
     session is error */
  assert_int32(NGHTTP2_ERR_PROTO, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, -1, NULL,
                                      reqnv, ARRLEN(reqnv), NULL));

  /* Sending stream ID <= 0 is error */
  assert_int32(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, 0, NULL,
                                      resnv, ARRLEN(resnv), NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers_continuation(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv nv[] = {
    MAKE_NV("h1", ""), MAKE_NV("h1", ""), MAKE_NV("h1", ""), MAKE_NV("h1", ""),
    MAKE_NV("h1", ""), MAKE_NV("h1", ""), MAKE_NV("h1", ""),
  };
  nghttp2_outbound_item *item;
  uint8_t data[4096];
  size_t i;
  my_user_data ud;

  memset(data, '0', sizeof(data));
  for (i = 0; i < ARRLEN(nv); ++i) {
    nv[i].valuelen = sizeof(data);
    nv[i].value = data;
  }

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, &ud));
  assert_int32(1, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, -1,
                                      NULL, nv, ARRLEN(nv), NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_uint8((NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS), ==,
               item->frame.hd.flags);
  assert_false(item->frame.hd.flags & NGHTTP2_FLAG_PRIORITY);

  ud.frame_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_headers_continuation_extra_large(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv nv[] = {
    MAKE_NV("h1", ""), MAKE_NV("h1", ""), MAKE_NV("h1", ""),
    MAKE_NV("h1", ""), MAKE_NV("h1", ""), MAKE_NV("h1", ""),
  };
  nghttp2_outbound_item *item;
  uint8_t data[16384];
  size_t i;
  my_user_data ud;
  nghttp2_option *opt;

  memset(data, '0', sizeof(data));
  for (i = 0; i < ARRLEN(nv); ++i) {
    nv[i].valuelen = sizeof(data);
    nv[i].value = data;
  }

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  /* The default size of max send header block length is too small to
     send these header fields.  Expand it. */
  nghttp2_option_new(&opt);
  nghttp2_option_set_max_send_header_block_length(opt, 102400);

  assert_int(0, ==,
             nghttp2_session_client_new2(&session, &callbacks, &ud, opt));
  assert_int32(1, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, -1,
                                      NULL, nv, ARRLEN(nv), NULL));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_uint8((NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS), ==,
               item->frame.hd.flags);
  assert_false(item->frame.hd.flags & NGHTTP2_FLAG_PRIORITY);

  ud.frame_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);

  nghttp2_session_del(session);
  nghttp2_option_del(opt);
}

void test_nghttp2_submit_settings(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_frame *frame;
  nghttp2_settings_entry iv[7];
  nghttp2_frame ack_frame;
  const int32_t UNKNOWN_ID = 1000000007;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 5;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16 * 1024;

  iv[2].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[2].value = 50;

  iv[3].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[3].value = 111;

  iv[4].settings_id = UNKNOWN_ID;
  iv[4].value = 999;

  iv[5].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[5].value = 1023;

  iv[6].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[6].value = (uint32_t)NGHTTP2_MAX_WINDOW_SIZE + 1;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  nghttp2_session_server_new(&session, &callbacks, &ud);

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 7));

  /* Make sure that local settings are not changed */
  assert_uint32(NGHTTP2_DEFAULT_MAX_CONCURRENT_STREAMS, ==,
                session->local_settings.max_concurrent_streams);
  assert_uint32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
                session->local_settings.initial_window_size);

  /* Now sends without 6th one */
  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 6));

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_SETTINGS, ==, item->frame.hd.type);

  frame = &item->frame;
  assert_size(6, ==, frame->settings.niv);
  assert_uint32(5, ==, frame->settings.iv[0].value);
  assert_int32(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, ==,
               frame->settings.iv[0].settings_id);

  assert_uint32(16 * 1024, ==, frame->settings.iv[1].value);
  assert_int32(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, ==,
               frame->settings.iv[1].settings_id);

  assert_int32(UNKNOWN_ID, ==, frame->settings.iv[4].settings_id);
  assert_uint32(999, ==, frame->settings.iv[4].value);

  ud.frame_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);

  assert_uint32(50, ==, session->pending_local_max_concurrent_stream);

  /* before receiving SETTINGS ACK, local settings have still default
     values */
  assert_uint32(NGHTTP2_DEFAULT_MAX_CONCURRENT_STREAMS, ==,
                nghttp2_session_get_local_settings(
                  session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));
  assert_uint32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
                nghttp2_session_get_local_settings(
                  session, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE));

  nghttp2_frame_settings_init(&ack_frame.settings, NGHTTP2_FLAG_ACK, NULL, 0);
  assert_int(0, ==,
             nghttp2_session_on_settings_received(session, &ack_frame, 0));
  nghttp2_frame_settings_free(&ack_frame.settings, mem);

  assert_uint32(16 * 1024, ==, session->local_settings.initial_window_size);
  assert_size(111, ==, session->hd_inflater.ctx.hd_table_bufsize_max);
  assert_size(111, ==, session->hd_inflater.min_hd_table_bufsize_max);
  assert_uint32(50, ==, session->local_settings.max_concurrent_streams);

  assert_uint32(50, ==,
                nghttp2_session_get_local_settings(
                  session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS));
  assert_uint32(16 * 1024, ==,
                nghttp2_session_get_local_settings(
                  session, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE));

  /* We just keep the last seen value */
  assert_uint32(50, ==, session->pending_local_max_concurrent_stream);

  nghttp2_session_del(session);

  /* Bail out if there are contradicting
     SETTINGS_NO_RFC7540_PRIORITIES in one SETTINGS. */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  iv[0].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[0].value = 1;
  iv[1].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[1].value = 0;

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 2));

  nghttp2_session_del(session);

  /* Attempt to change SETTINGS_NO_RFC7540_PRIORITIES in the 2nd
     SETTINGS. */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  iv[0].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[0].value = 1;

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));

  iv[0].settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv[0].value = 0;

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_settings_update_local_window_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_settings_entry iv[4];
  nghttp2_stream *stream;
  nghttp2_frame ack_frame;
  nghttp2_mem *mem;
  nghttp2_option *option;

  mem = nghttp2_mem_default();
  nghttp2_frame_settings_init(&ack_frame.settings, NGHTTP2_FLAG_ACK, NULL, 0);

  iv[0].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[0].value = 16 * 1024;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  stream = open_recv_stream(session, 1);
  stream->local_window_size = NGHTTP2_INITIAL_WINDOW_SIZE + 100;
  stream->recv_window_size = 32768;

  open_recv_stream(session, 3);

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==,
             nghttp2_session_on_settings_received(session, &ack_frame, 0));

  stream = nghttp2_session_get_stream(session, 1);
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(16 * 1024 + 100, ==, stream->local_window_size);

  stream = nghttp2_session_get_stream(session, 3);
  assert_int32(16 * 1024, ==, stream->local_window_size);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(32768, ==, item->frame.window_update.window_size_increment);

  nghttp2_session_del(session);

  /* Without auto-window update */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_window_update(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  nghttp2_option_del(option);

  stream = open_recv_stream(session, 1);
  stream->local_window_size = NGHTTP2_INITIAL_WINDOW_SIZE + 100;
  stream->recv_window_size = 32768;

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==,
             nghttp2_session_on_settings_received(session, &ack_frame, 0));

  stream = nghttp2_session_get_stream(session, 1);

  assert_int32(32768, ==, stream->recv_window_size);
  assert_int32(16 * 1024 + 100, ==, stream->local_window_size);
  /* Check that we can handle the case where local_window_size <
     recv_window_size */
  assert_int32(0, ==, nghttp2_session_get_stream_local_window_size(session, 1));

  nghttp2_session_del(session);

  /* Check overflow case */
  iv[0].value = 128 * 1024;
  nghttp2_session_server_new(&session, &callbacks, NULL);
  stream = open_recv_stream(session, 1);
  stream->local_window_size = NGHTTP2_MAX_WINDOW_SIZE;

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==,
             nghttp2_session_on_settings_received(session, &ack_frame, 0));

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_FLOW_CONTROL_ERROR, ==,
                item->frame.rst_stream.error_code);

  nghttp2_session_del(session);
  nghttp2_frame_settings_free(&ack_frame.settings, mem);
}

void test_nghttp2_submit_settings_multiple_times(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_settings_entry iv[4];
  nghttp2_frame frame;
  nghttp2_inflight_settings *inflight_settings;

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  /* first SETTINGS */
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 100;

  iv[1].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
  iv[1].value = 0;

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 2));

  inflight_settings = session->inflight_settings_head;

  assert_not_null(inflight_settings);
  assert_int32(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, ==,
               inflight_settings->iv[0].settings_id);
  assert_uint32(100, ==, inflight_settings->iv[0].value);
  assert_size(2, ==, inflight_settings->niv);
  assert_null(inflight_settings->next);

  assert_uint32(100, ==, session->pending_local_max_concurrent_stream);
  assert_uint8(0, ==, session->pending_enable_push);

  /* second SETTINGS */
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 99;

  assert_int(0, ==, nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 1));

  inflight_settings = session->inflight_settings_head->next;

  assert_not_null(inflight_settings);
  assert_int32(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, ==,
               inflight_settings->iv[0].settings_id);
  assert_uint32(99, ==, inflight_settings->iv[0].value);
  assert_size(1, ==, inflight_settings->niv);
  assert_null(inflight_settings->next);

  assert_uint32(99, ==, session->pending_local_max_concurrent_stream);
  assert_uint8(0, ==, session->pending_enable_push);

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_ACK, NULL, 0);

  /* receive SETTINGS ACK */
  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  inflight_settings = session->inflight_settings_head;

  /* first inflight SETTINGS was removed */
  assert_not_null(inflight_settings);
  assert_int32(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, ==,
               inflight_settings->iv[0].settings_id);
  assert_uint32(99, ==, inflight_settings->iv[0].value);
  assert_size(1, ==, inflight_settings->niv);
  assert_null(inflight_settings->next);

  assert_uint32(100, ==, session->local_settings.max_concurrent_streams);

  /* receive SETTINGS ACK again */
  assert_int(0, ==, nghttp2_session_on_settings_received(session, &frame, 0));

  assert_null(session->inflight_settings_head);
  assert_uint32(99, ==, session->local_settings.max_concurrent_streams);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_push_promise(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));
  open_recv_stream(session, 1);
  assert_int32(2, ==,
               nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 1, reqnv,
                                           ARRLEN(reqnv), &ud));

  stream = nghttp2_session_get_stream(session, 2);

  assert_not_null(stream);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_RESERVED, ==, stream->state);
  assert_ptr_equal(&ud, nghttp2_session_get_stream_user_data(session, 2));

  ud.frame_send_cb_called = 0;
  ud.sent_frame_type = 0;

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_PUSH_PROMISE, ==, ud.sent_frame_type);

  stream = nghttp2_session_get_stream(session, 2);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_RESERVED, ==, stream->state);
  assert_ptr_equal(&ud, nghttp2_session_get_stream_user_data(session, 2));

  /* submit PUSH_PROMISE while associated stream is not opened */
  assert_int32(NGHTTP2_ERR_STREAM_CLOSED, ==,
               nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 3, reqnv,
                                           ARRLEN(reqnv), NULL));

  /* Stream ID <= 0 is error */
  assert_int32(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
               nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 0, reqnv,
                                           ARRLEN(reqnv), NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_window_update(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);
  stream = open_recv_stream(session, 2);
  stream->recv_window_size = 4096;

  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2, 1024));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(1024, ==, item->frame.window_update.window_size_increment);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(3072, ==, stream->recv_window_size);

  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2, 4096));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(4096, ==, item->frame.window_update.window_size_increment);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(0, ==, stream->recv_window_size);

  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2, 4096));
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(4096, ==, item->frame.window_update.window_size_increment);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int32(0, ==, stream->recv_window_size);

  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2, 0));
  /* It is ok if stream is closed or does not exist at the call
     time */
  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 4, 4096));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_window_update_local_window_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);
  stream = open_recv_stream(session, 2);
  stream->recv_window_size = 4096;

  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2,
                                          stream->recv_window_size + 1));
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 1, ==, stream->local_window_size);
  assert_int32(0, ==, stream->recv_window_size);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(4097, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Let's decrement local window size */
  stream->recv_window_size = 4096;
  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2,
                                          -stream->local_window_size / 2));
  assert_int32(32768, ==, stream->local_window_size);
  assert_int32(-28672, ==, stream->recv_window_size);
  assert_int32(32768, ==, stream->recv_reduction);

  item = nghttp2_session_get_next_ob_item(session);
  assert_null(item);

  /* Increase local window size */
  assert_int(
    0, ==, nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2, 16384));
  assert_int32(49152, ==, stream->local_window_size);
  assert_int32(-12288, ==, stream->recv_window_size);
  assert_int32(16384, ==, stream->recv_reduction);
  assert_null(nghttp2_session_get_next_ob_item(session));

  assert_int(NGHTTP2_ERR_FLOW_CONTROL, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 2,
                                          NGHTTP2_MAX_WINDOW_SIZE));

  assert_int(0, ==, nghttp2_session_send(session));

  /* Check connection-level flow control */
  session->recv_window_size = 4096;
  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0,
                                          session->recv_window_size + 1));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1, ==,
               session->local_window_size);
  assert_int32(0, ==, session->recv_window_size);
  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(4097, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Go decrement part */
  session->recv_window_size = 4096;
  assert_int(0, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0,
                                          -session->local_window_size / 2));
  assert_int32(32768, ==, session->local_window_size);
  assert_int32(-28672, ==, session->recv_window_size);
  assert_int32(32768, ==, session->recv_reduction);
  item = nghttp2_session_get_next_ob_item(session);
  assert_null(item);

  /* Increase local window size */
  assert_int(
    0, ==, nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0, 16384));
  assert_int32(49152, ==, session->local_window_size);
  assert_int32(-12288, ==, session->recv_window_size);
  assert_int32(16384, ==, session->recv_reduction);
  assert_null(nghttp2_session_get_next_ob_item(session));

  assert_int(NGHTTP2_ERR_FLOW_CONTROL, ==,
             nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0,
                                          NGHTTP2_MAX_WINDOW_SIZE));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_shutdown_notice(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  assert_int(0, ==, nghttp2_submit_shutdown_notice(session));

  ud.frame_send_cb_called = 0;

  nghttp2_session_send(session);

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_GOAWAY, ==, ud.sent_frame_type);
  assert_int32((1u << 31) - 1, ==, session->local_last_stream_id);

  /* After another GOAWAY, nghttp2_submit_shutdown_notice() is
     noop. */
  assert_int(0, ==,
             nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR));

  ud.frame_send_cb_called = 0;

  nghttp2_session_send(session);

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_GOAWAY, ==, ud.sent_frame_type);
  assert_int32(0, ==, session->local_last_stream_id);

  assert_int(0, ==, nghttp2_submit_shutdown_notice(session));

  ud.frame_send_cb_called = 0;
  ud.frame_not_send_cb_called = 0;

  nghttp2_session_send(session);

  assert_int(0, ==, ud.frame_send_cb_called);
  assert_int(0, ==, ud.frame_not_send_cb_called);

  nghttp2_session_del(session);

  /* Using nghttp2_submit_shutdown_notice() with client side session
     is error */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  assert_int(NGHTTP2_ERR_INVALID_STATE, ==,
             nghttp2_submit_shutdown_notice(session));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_invalid_nv(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv empty_name_nv[] = {MAKE_NV("Version", "HTTP/1.1"),
                                MAKE_NV("", "empty name")};

  /* Now invalid header name/value pair in HTTP/1.1 is accepted in
     nghttp2 */

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, NULL));

  /* nghttp2_submit_response */
  assert_int(0, ==,
             nghttp2_submit_response2(session, 2, empty_name_nv,
                                      ARRLEN(empty_name_nv), NULL));

  /* nghttp2_submit_push_promise */
  open_recv_stream(session, 1);

  assert_int32(0, <,
               nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 1,
                                           empty_name_nv, ARRLEN(empty_name_nv),
                                           NULL));

  nghttp2_session_del(session);

  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, NULL));

  /* nghttp2_submit_request */
  assert_int32(0, <,
               nghttp2_submit_request2(session, NULL, empty_name_nv,
                                       ARRLEN(empty_name_nv), NULL, NULL));

  /* nghttp2_submit_headers */
  assert_int32(0, <,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, -1, NULL,
                                      empty_name_nv, ARRLEN(empty_name_nv),
                                      NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_submit_extension(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  accumulator acc;
  nghttp2_mem *mem;
  const char data[] = "Hello World!";
  size_t len;
  int32_t stream_id;
  int rv;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  callbacks.pack_extension_callback2 = pack_extension_callback;
  callbacks.send_callback2 = accumulator_send_callback;

  nghttp2_buf_init2(&ud.scratchbuf, 4096, mem);

  nghttp2_session_client_new(&session, &callbacks, &ud);

  ud.scratchbuf.last = nghttp2_cpymem(ud.scratchbuf.last, data, sizeof(data));
  ud.acc = &acc;

  rv = nghttp2_submit_extension(session, 211, 0x01, 3, &ud.scratchbuf);

  assert_int(0, ==, rv);

  acc.length = 0;

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN + sizeof(data), ==, acc.length);

  len = nghttp2_get_uint32(acc.buf) >> 8;

  assert_size(sizeof(data), ==, len);
  assert_uint8(211, ==, acc.buf[3]);
  assert_uint8(0x01, ==, acc.buf[4]);

  stream_id = (int32_t)nghttp2_get_uint32(acc.buf + 5);

  assert_int32(3, ==, stream_id);
  assert_memory_equal(sizeof(data), data, &acc.buf[NGHTTP2_FRAME_HDLEN]);

  nghttp2_session_del(session);

  /* submitting standard HTTP/2 frame is error */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = nghttp2_submit_extension(session, NGHTTP2_GOAWAY, NGHTTP2_FLAG_NONE, 0,
                                NULL);

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);

  nghttp2_session_del(session);
  nghttp2_buf_free(&ud.scratchbuf, mem);
}

void test_nghttp2_submit_altsvc(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  int rv;
  nghttp2_ssize len;
  const uint8_t *data;
  nghttp2_frame_hd hd;
  size_t origin_len;
  const uint8_t origin[] = "nghttp2.org";
  const uint8_t field_value[] = "h2=\":443\"";

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = nghttp2_submit_altsvc(session, NGHTTP2_FLAG_NONE, 0, origin,
                             sizeof(origin) - 1, field_value,
                             sizeof(field_value) - 1);

  assert_int(0, ==, rv);

  ud.frame_send_cb_called = 0;

  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(NGHTTP2_FRAME_HDLEN + 2 + sizeof(origin) - 1 +
                   sizeof(field_value) - 1,
                 ==, len);

  nghttp2_frame_unpack_frame_hd(&hd, data);

  assert_size(2 + sizeof(origin) - 1 + sizeof(field_value) - 1, ==, hd.length);
  assert_uint8(NGHTTP2_ALTSVC, ==, hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);

  origin_len = nghttp2_get_uint16(data + NGHTTP2_FRAME_HDLEN);

  assert_size(sizeof(origin) - 1, ==, origin_len);
  assert_memory_equal(sizeof(origin) - 1, origin,
                      data + NGHTTP2_FRAME_HDLEN + 2);
  assert_memory_equal(hd.length - (sizeof(origin) - 1) - 2, field_value,
                      data + NGHTTP2_FRAME_HDLEN + 2 + sizeof(origin) - 1);

  /* submitting empty origin with stream_id == 0 is error */
  rv = nghttp2_submit_altsvc(session, NGHTTP2_FLAG_NONE, 0, NULL, 0,
                             field_value, sizeof(field_value) - 1);

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);

  /* submitting non-empty origin with stream_id != 0 is error */
  rv = nghttp2_submit_altsvc(session, NGHTTP2_FLAG_NONE, 1, origin,
                             sizeof(origin) - 1, field_value,
                             sizeof(field_value) - 1);

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);

  nghttp2_session_del(session);

  /* submitting from client side session is error */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  rv = nghttp2_submit_altsvc(session, NGHTTP2_FLAG_NONE, 0, origin,
                             sizeof(origin) - 1, field_value,
                             sizeof(field_value) - 1);

  assert_int(NGHTTP2_ERR_INVALID_STATE, ==, rv);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_origin(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  int rv;
  nghttp2_ssize len;
  const uint8_t *data;
  static const uint8_t nghttp2[] = "https://nghttp2.org";
  static const uint8_t examples[] = "https://examples.com";
  static const nghttp2_origin_entry ov[] = {
    {
      (uint8_t *)nghttp2,
      sizeof(nghttp2) - 1,
    },
    {
      (uint8_t *)examples,
      sizeof(examples) - 1,
    },
  };
  nghttp2_frame frame;
  nghttp2_ext_origin origin;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_send_callback = on_frame_send_callback;

  frame.ext.payload = &origin;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = nghttp2_submit_origin(session, NGHTTP2_FLAG_NONE, ov, 2);

  assert_int(0, ==, rv);

  ud.frame_send_cb_called = 0;
  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, len);
  assert_int(1, ==, ud.frame_send_cb_called);

  nghttp2_frame_unpack_frame_hd(&frame.hd, data);
  rv =
    nghttp2_frame_unpack_origin_payload(&frame.ext, data + NGHTTP2_FRAME_HDLEN,
                                        (size_t)len - NGHTTP2_FRAME_HDLEN, mem);

  assert_int(0, ==, rv);
  assert_int32(0, ==, frame.hd.stream_id);
  assert_uint8(NGHTTP2_ORIGIN, ==, frame.hd.type);
  assert_size(2, ==, origin.nov);
  assert_memory_equal(sizeof(nghttp2) - 1, nghttp2, origin.ov[0].origin);
  assert_size(sizeof(nghttp2) - 1, ==, origin.ov[0].origin_len);
  assert_memory_equal(sizeof(examples) - 1, examples, origin.ov[1].origin);
  assert_size(sizeof(examples) - 1, ==, origin.ov[1].origin_len);

  nghttp2_frame_origin_free(&frame.ext, mem);

  nghttp2_session_del(session);

  /* Submitting ORIGIN frame from client session is error */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  rv = nghttp2_submit_origin(session, NGHTTP2_FLAG_NONE, ov, 1);

  assert_int(NGHTTP2_ERR_INVALID_STATE, ==, rv);

  nghttp2_session_del(session);

  /* Submitting empty ORIGIN frame */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = nghttp2_submit_origin(session, NGHTTP2_FLAG_NONE, NULL, 0);

  assert_int(0, ==, rv);

  ud.frame_send_cb_called = 0;
  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(NGHTTP2_FRAME_HDLEN, ==, len);
  assert_int(1, ==, ud.frame_send_cb_called);

  nghttp2_frame_unpack_frame_hd(&frame.hd, data);

  assert_uint8(NGHTTP2_ORIGIN, ==, frame.hd.type);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_priority_update(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  const uint8_t field_value[] = "i";
  my_user_data ud;
  const uint8_t *data;
  int rv;
  nghttp2_frame frame;
  nghttp2_ext_priority_update priority_update;
  nghttp2_ssize len;
  int32_t stream_id;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_send_callback = on_frame_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  session->pending_no_rfc7540_priorities = 1;

  stream_id =
    nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int32(1, ==, stream_id);

  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, len);

  rv = nghttp2_submit_priority_update(session, NGHTTP2_FLAG_NONE, stream_id,
                                      field_value, sizeof(field_value) - 1);

  assert_int(0, ==, rv);

  frame.ext.payload = &priority_update;

  ud.frame_send_cb_called = 0;
  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, len);
  assert_int(1, ==, ud.frame_send_cb_called);

  nghttp2_frame_unpack_frame_hd(&frame.hd, data);
  nghttp2_frame_unpack_priority_update_payload(
    &frame.ext, (uint8_t *)(data + NGHTTP2_FRAME_HDLEN),
    (size_t)len - NGHTTP2_FRAME_HDLEN);

  assert_int32(0, ==, frame.hd.stream_id);
  assert_uint8(NGHTTP2_PRIORITY_UPDATE, ==, frame.hd.type);
  assert_int32(stream_id, ==, priority_update.stream_id);
  assert_size(sizeof(field_value) - 1, ==, priority_update.field_value_len);
  assert_memory_equal(sizeof(field_value) - 1, field_value,
                      priority_update.field_value);

  nghttp2_session_del(session);

  /* Submitting PRIORITY_UPDATE frame from server session is error */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 1);

  rv = nghttp2_submit_priority_update(session, NGHTTP2_FLAG_NONE, 1,
                                      field_value, sizeof(field_value) - 1);

  assert_int(NGHTTP2_ERR_INVALID_STATE, ==, rv);

  nghttp2_session_del(session);

  /* Submitting PRIORITY_UPDATE with empty field_value */
  nghttp2_session_client_new(&session, &callbacks, &ud);

  stream_id =
    nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int32(1, ==, stream_id);

  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, len);

  rv = nghttp2_submit_priority_update(session, NGHTTP2_FLAG_NONE, stream_id,
                                      NULL, 0);

  assert_int(0, ==, rv);

  frame.ext.payload = &priority_update;

  len = nghttp2_session_mem_send2(session, &data);

  assert_ptrdiff(0, <, len);

  nghttp2_frame_unpack_frame_hd(&frame.hd, data);
  nghttp2_frame_unpack_priority_update_payload(
    &frame.ext, (uint8_t *)(data + NGHTTP2_FRAME_HDLEN),
    (size_t)len - NGHTTP2_FRAME_HDLEN);

  assert_int32(0, ==, frame.hd.stream_id);
  assert_uint8(NGHTTP2_PRIORITY_UPDATE, ==, frame.hd.type);
  assert_int32(stream_id, ==, priority_update.stream_id);
  assert_size(0, ==, priority_update.field_value_len);
  assert_null(priority_update.field_value);

  nghttp2_session_del(session);
}

void test_nghttp2_submit_rst_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  int rv;
  int32_t stream_id;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  /* Sending RST_STREAM to idle stream (local) is ignored */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  rv =
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 1, NGHTTP2_NO_ERROR);

  assert_int(0, ==, rv);

  item = nghttp2_outbound_queue_top(&session->ob_reg);

  assert_null(item);

  nghttp2_session_del(session);

  /* Sending RST_STREAM to idle stream (remote) is ignored */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  rv =
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 2, NGHTTP2_NO_ERROR);

  assert_int(0, ==, rv);

  item = nghttp2_outbound_queue_top(&session->ob_reg);

  assert_null(item);

  nghttp2_session_del(session);

  /* Sending RST_STREAM to non-idle stream (local) */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  open_sent_stream(session, 1);

  rv =
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 1, NGHTTP2_NO_ERROR);

  assert_int(0, ==, rv);

  item = nghttp2_outbound_queue_top(&session->ob_reg);

  assert_not_null(item);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.hd.stream_id);

  nghttp2_session_del(session);

  /* Sending RST_STREAM to non-idle stream (remote) */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  open_recv_stream(session, 2);

  rv =
    nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 2, NGHTTP2_NO_ERROR);

  assert_int(0, ==, rv);

  item = nghttp2_outbound_queue_top(&session->ob_reg);

  assert_not_null(item);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(2, ==, item->frame.hd.stream_id);

  nghttp2_session_del(session);

  /* Sending RST_STREAM to pending stream */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  stream_id =
    nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int32(0, <, stream_id);

  item = nghttp2_outbound_queue_top(&session->ob_syn);

  assert_not_null(item);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_false(item->aux_data.headers.canceled);

  rv = nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                                 NGHTTP2_NO_ERROR);

  assert_int(0, ==, rv);

  item = nghttp2_outbound_queue_top(&session->ob_syn);

  assert_not_null(item);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_true(item->aux_data.headers.canceled);

  nghttp2_session_del(session);
}

void test_nghttp2_session_open_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  nghttp2_session_server_new(&session, &callbacks, NULL);

  stream = nghttp2_session_open_stream(session, 1, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_OPENED, NULL);
  assert_size(1, ==, session->num_incoming_streams);
  assert_size(0, ==, session->num_outgoing_streams);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENED, ==, stream->state);
  assert_uint8(NGHTTP2_SHUT_NONE, ==, stream->shut_flags);

  stream = nghttp2_session_open_stream(session, 2, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_OPENING, NULL);
  assert_size(1, ==, session->num_incoming_streams);
  assert_size(1, ==, session->num_outgoing_streams);
  assert_uint8(NGHTTP2_SHUT_NONE, ==, stream->shut_flags);

  stream = nghttp2_session_open_stream(session, 4, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_RESERVED, NULL);
  assert_size(1, ==, session->num_incoming_streams);
  assert_size(1, ==, session->num_outgoing_streams);
  assert_uint8(NGHTTP2_SHUT_RD, ==, stream->shut_flags);

  stream = nghttp2_session_open_stream(session, 3, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_OPENED, NULL);

  /* Dependency to idle stream */
  stream = nghttp2_session_open_stream(session, 5, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_OPENED, NULL);

  /* Dependency to closed stream which is not in dependency tree */
  session->last_recv_stream_id = 7;

  stream = nghttp2_session_open_stream(session, 9, NGHTTP2_FLAG_NONE,
                                       NGHTTP2_STREAM_OPENED, NULL);

  nghttp2_session_del(session);

  nghttp2_session_client_new(&session, &callbacks, NULL);
  stream = nghttp2_session_open_stream(session, 4, NGHTTP2_STREAM_FLAG_NONE,
                                       NGHTTP2_STREAM_RESERVED, NULL);
  assert_size(0, ==, session->num_incoming_streams);
  assert_size(0, ==, session->num_outgoing_streams);
  assert_uint8(NGHTTP2_SHUT_WR, ==, stream->shut_flags);

  nghttp2_session_del(session);
}

void test_nghttp2_session_get_next_ob_item(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_priority_spec pri_spec;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);
  session->remote_settings.max_concurrent_streams = 2;

  assert_null(nghttp2_session_get_next_ob_item(session));
  nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL);
  assert_uint8(NGHTTP2_PING, ==,
               nghttp2_session_get_next_ob_item(session)->frame.hd.type);

  assert_int32(1, ==,
               nghttp2_submit_request2(session, NULL, NULL, 0, NULL, NULL));
  assert_uint8(NGHTTP2_PING, ==,
               nghttp2_session_get_next_ob_item(session)->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_null(nghttp2_session_get_next_ob_item(session));

  /* Incoming stream does not affect the number of outgoing max
     concurrent streams. */
  open_recv_stream(session, 2);

  nghttp2_priority_spec_init(&pri_spec, 0, NGHTTP2_MAX_WEIGHT, 0);

  assert_int(3, ==,
             nghttp2_submit_request2(session, &pri_spec, NULL, 0, NULL, NULL));
  assert_uint8(NGHTTP2_HEADERS, ==,
               nghttp2_session_get_next_ob_item(session)->frame.hd.type);
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(5, ==,
             nghttp2_submit_request2(session, &pri_spec, NULL, 0, NULL, NULL));
  assert_null(nghttp2_session_get_next_ob_item(session));

  session->remote_settings.max_concurrent_streams = 3;

  assert_uint8(NGHTTP2_HEADERS, ==,
               nghttp2_session_get_next_ob_item(session)->frame.hd.type);

  nghttp2_session_del(session);

  /* Check that push reply HEADERS are queued into ob_ss_pq */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  session->remote_settings.max_concurrent_streams = 0;
  open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 2, NULL,
                                      NULL, 0, NULL));
  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_size(1, ==, nghttp2_outbound_queue_size(&session->ob_syn));
  nghttp2_session_del(session);
}

void test_nghttp2_session_pop_next_ob_item(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_priority_spec pri_spec;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);
  session->remote_settings.max_concurrent_streams = 1;

  assert_null(nghttp2_session_pop_next_ob_item(session));

  nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL);

  nghttp2_priority_spec_init(&pri_spec, 0, 254, 0);

  nghttp2_submit_request2(session, &pri_spec, NULL, 0, NULL, NULL);

  item = nghttp2_session_pop_next_ob_item(session);
  assert_uint8(NGHTTP2_PING, ==, item->frame.hd.type);
  nghttp2_outbound_item_free(item, mem);
  mem->free(item, NULL);

  item = nghttp2_session_pop_next_ob_item(session);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  nghttp2_outbound_item_free(item, mem);
  mem->free(item, NULL);

  assert_null(nghttp2_session_pop_next_ob_item(session));

  /* Incoming stream does not affect the number of outgoing max
     concurrent streams. */
  open_recv_stream(session, 4);
  /* In-flight outgoing stream */
  open_sent_stream(session, 1);

  nghttp2_priority_spec_init(&pri_spec, 0, NGHTTP2_MAX_WEIGHT, 0);

  nghttp2_submit_request2(session, &pri_spec, NULL, 0, NULL, NULL);

  assert_null(nghttp2_session_pop_next_ob_item(session));

  session->remote_settings.max_concurrent_streams = 2;

  item = nghttp2_session_pop_next_ob_item(session);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  nghttp2_outbound_item_free(item, mem);
  mem->free(item, NULL);

  nghttp2_session_del(session);

  /* Check that push reply HEADERS are queued into ob_ss_pq */
  nghttp2_session_server_new(&session, &callbacks, NULL);
  session->remote_settings.max_concurrent_streams = 0;
  open_sent_stream2(session, 2, NGHTTP2_STREAM_RESERVED);
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 2, NULL,
                                      NULL, 0, NULL));
  assert_null(nghttp2_session_pop_next_ob_item(session));
  assert_size(1, ==, nghttp2_outbound_queue_size(&session->ob_syn));
  nghttp2_session_del(session);
}

void test_nghttp2_session_reply_fail(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = fail_send_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = 4 * 1024;
  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  assert_int(0, ==, nghttp2_submit_response2(session, 1, NULL, 0, &data_prd));
  assert_int(NGHTTP2_ERR_CALLBACK_FAILURE, ==, nghttp2_session_send(session));
  nghttp2_session_del(session);
}

void test_nghttp2_session_max_concurrent_streams(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_frame frame;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);
  open_recv_stream(session, 1);

  /* Check un-ACKed SETTINGS_MAX_CONCURRENT_STREAMS */
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 3,
                             NGHTTP2_HCAT_HEADERS, NULL, NULL, 0);
  session->pending_local_max_concurrent_stream = 1;

  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));

  item = nghttp2_outbound_queue_top(&session->ob_reg);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_REFUSED_STREAM, ==, item->frame.rst_stream.error_code);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Check ACKed SETTINGS_MAX_CONCURRENT_STREAMS */
  session->local_settings.max_concurrent_streams = 1;
  frame.hd.stream_id = 5;

  assert_int(NGHTTP2_ERR_IGN_HEADER_BLOCK, ==,
             nghttp2_session_on_request_headers_received(session, &frame));

  item = nghttp2_outbound_queue_top(&session->ob_reg);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, item->frame.goaway.error_code);

  nghttp2_frame_headers_free(&frame.headers, mem);
  nghttp2_session_del(session);
}

void test_nghttp2_session_stop_data_with_rst_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_data_provider2 data_prd;
  nghttp2_frame frame;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.send_callback2 = block_count_send_callback;
  data_prd.read_callback = fixed_length_data_source_read_callback;

  ud.frame_send_cb_called = 0;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 4;

  nghttp2_session_server_new(&session, &callbacks, &ud);
  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  nghttp2_submit_response2(session, 1, NULL, 0, &data_prd);

  ud.block_count = 2;
  /* Sends response HEADERS + DATA[0] */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_DATA, ==, ud.sent_frame_type);
  /* data for DATA[1] is read from data_prd but it is not sent */
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 2);

  nghttp2_frame_rst_stream_init(&frame.rst_stream, 1, NGHTTP2_CANCEL);
  assert_int(0, ==, nghttp2_session_on_rst_stream_received(session, &frame));
  nghttp2_frame_rst_stream_free(&frame.rst_stream);

  /* Big enough number to send all DATA frames potentially. */
  ud.block_count = 100;
  /* Nothing will be sent in the following call. */
  assert_int(0, ==, nghttp2_session_send(session));
  /* With RST_STREAM, stream is canceled and further DATA on that
     stream are not sent. */
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 2);

  assert_null(nghttp2_session_get_stream(session, 1));

  nghttp2_session_del(session);
}

void test_nghttp2_session_defer_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_data_provider2 data_prd;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.send_callback2 = block_count_send_callback;
  data_prd.read_callback = defer_data_source_read_callback;

  ud.frame_send_cb_called = 0;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 4;

  nghttp2_session_server_new(&session, &callbacks, &ud);
  stream = open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  session->remote_window_size = 1 << 20;
  stream->remote_window_size = 1 << 20;

  nghttp2_submit_response2(session, 1, NULL, 0, &data_prd);

  ud.block_count = 1;
  /* Sends HEADERS reply */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_HEADERS, ==, ud.sent_frame_type);
  /* No data is read */
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 4);

  ud.block_count = 1;
  nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL);
  /* Sends PING */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_PING, ==, ud.sent_frame_type);

  /* Resume deferred DATA */
  assert_int(0, ==, nghttp2_session_resume_data(session, 1));
  item = stream->item;
  item->aux_data.data.dpw.data_prd.v1.read_callback =
    fixed_length_data_source_read_callback;
  ud.block_count = 1;
  /* Reads 2 DATA chunks */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 2);

  /* Deferred again */
  item->aux_data.data.dpw.data_prd.v1.read_callback =
    defer_data_source_read_callback;
  /* This is needed since 16KiB block is already read and waiting to be
     sent. No read_callback invocation. */
  ud.block_count = 1;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 2);

  /* Resume deferred DATA */
  assert_int(0, ==, nghttp2_session_resume_data(session, 1));
  item->aux_data.data.dpw.data_prd.v1.read_callback =
    fixed_length_data_source_read_callback;
  ud.block_count = 1;
  /* Reads 2 16KiB blocks */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(ud.data_source_length, ==, 0);

  nghttp2_session_del(session);
}

void test_nghttp2_session_flow_control(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_data_provider2 data_prd;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  int32_t new_initial_window_size;
  nghttp2_settings_entry iv[1];
  nghttp2_frame settings_frame;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = fixed_bytes_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  data_prd.read_callback = fixed_length_data_source_read_callback;

  ud.frame_send_cb_called = 0;
  ud.data_source_length = 128 * 1024;
  /* Use smaller emission count so that we can check outbound flow
     control window calculation is correct. */
  ud.fixed_sendlen = 2 * 1024;

  /* Initial window size to 64KiB - 1*/
  nghttp2_session_client_new(&session, &callbacks, &ud);
  /* Change it to 64KiB for easy calculation */
  session->remote_window_size = 64 * 1024;
  session->remote_settings.initial_window_size = 64 * 1024;

  nghttp2_submit_request2(session, NULL, NULL, 0, &data_prd, NULL);

  /* Sends 64KiB - 1 data */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(64 * 1024, ==, ud.data_source_length);

  /* Back 32KiB in stream window */
  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 1,
                                   32 * 1024);
  nghttp2_session_on_window_update_received(session, &frame);

  /* Send nothing because of connection-level window */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(64 * 1024, ==, ud.data_source_length);

  /* Back 32KiB in connection-level window */
  frame.hd.stream_id = 0;
  nghttp2_session_on_window_update_received(session, &frame);

  /* Sends another 32KiB data */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(32 * 1024, ==, ud.data_source_length);

  stream = nghttp2_session_get_stream(session, 1);
  /* Change initial window size to 16KiB. The window_size becomes
     negative. */
  new_initial_window_size = 16 * 1024;
  stream->remote_window_size =
    new_initial_window_size -
    ((int32_t)session->remote_settings.initial_window_size -
     stream->remote_window_size);
  session->remote_settings.initial_window_size =
    (uint32_t)new_initial_window_size;
  assert_int32(-48 * 1024, ==, stream->remote_window_size);

  /* Back 48KiB to stream window */
  frame.hd.stream_id = 1;
  frame.window_update.window_size_increment = 48 * 1024;
  nghttp2_session_on_window_update_received(session, &frame);

  /* Nothing is sent because window_size is 0 */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(32 * 1024, ==, ud.data_source_length);

  /* Back 16KiB in stream window */
  frame.hd.stream_id = 1;
  frame.window_update.window_size_increment = 16 * 1024;
  nghttp2_session_on_window_update_received(session, &frame);

  /* Back 24KiB in connection-level window */
  frame.hd.stream_id = 0;
  frame.window_update.window_size_increment = 24 * 1024;
  nghttp2_session_on_window_update_received(session, &frame);

  /* Sends another 16KiB data */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(16 * 1024, ==, ud.data_source_length);

  /* Increase initial window size to 32KiB */
  iv[0].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[0].value = 32 * 1024;

  nghttp2_frame_settings_init(&settings_frame.settings, NGHTTP2_FLAG_NONE,
                              dup_iv(iv, 1), 1);
  nghttp2_session_on_settings_received(session, &settings_frame, 1);
  nghttp2_frame_settings_free(&settings_frame.settings, mem);

  /* Sends another 8KiB data */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(8 * 1024, ==, ud.data_source_length);

  /* Back 8KiB in connection-level window */
  frame.hd.stream_id = 0;
  frame.window_update.window_size_increment = 8 * 1024;
  nghttp2_session_on_window_update_received(session, &frame);

  /* Sends last 8KiB data */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(0, ==, ud.data_source_length);
  assert_true(nghttp2_session_get_stream(session, 1)->shut_flags &
              NGHTTP2_SHUT_WR);

  nghttp2_frame_window_update_free(&frame.window_update);
  nghttp2_session_del(session);
}

void test_nghttp2_session_flow_control_data_recv(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  uint8_t data[64 * 1024 + 16];
  nghttp2_frame_hd hd;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  /* Initial window size to 64KiB - 1*/
  nghttp2_session_client_new(&session, &callbacks, NULL);

  stream = open_sent_stream(session, 1);

  nghttp2_stream_shutdown(stream, NGHTTP2_SHUT_WR);

  session->local_window_size = NGHTTP2_MAX_PAYLOADLEN;
  stream->local_window_size = NGHTTP2_MAX_PAYLOADLEN;

  /* Create DATA frame */
  memset(data, 0, sizeof(data));
  nghttp2_frame_hd_init(&hd, NGHTTP2_MAX_PAYLOADLEN, NGHTTP2_DATA,
                        NGHTTP2_FLAG_END_STREAM, 1);

  nghttp2_frame_pack_frame_hd(data, &hd);
  assert_ptrdiff(
    NGHTTP2_MAX_PAYLOADLEN + NGHTTP2_FRAME_HDLEN, ==,
    nghttp2_session_mem_recv2(session, data,
                              NGHTTP2_MAX_PAYLOADLEN + NGHTTP2_FRAME_HDLEN));

  item = nghttp2_session_get_next_ob_item(session);
  /* Since this is the last frame, stream-level WINDOW_UPDATE is not
     issued, but connection-level is. */
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.hd.stream_id);
  assert_int32(NGHTTP2_MAX_PAYLOADLEN, ==,
               item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Receive DATA for closed stream. They are still subject to under
     connection-level flow control, since this situation arises when
     RST_STREAM is issued by the remote, but the local side keeps
     sending DATA frames. Without calculating connection-level window,
     the subsequent flow control gets confused. */
  assert_ptrdiff(
    NGHTTP2_MAX_PAYLOADLEN + NGHTTP2_FRAME_HDLEN, ==,
    nghttp2_session_mem_recv2(session, data,
                              NGHTTP2_MAX_PAYLOADLEN + NGHTTP2_FRAME_HDLEN));

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.hd.stream_id);
  assert_int32(NGHTTP2_MAX_PAYLOADLEN, ==,
               item->frame.window_update.window_size_increment);

  nghttp2_session_del(session);
}

void test_nghttp2_session_flow_control_data_with_padding_recv(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  uint8_t data[1024];
  nghttp2_frame_hd hd;
  nghttp2_stream *stream;
  nghttp2_option *option;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_option_new(&option);
  /* Disable auto window update so that we can check padding is
     consumed automatically */
  nghttp2_option_set_no_auto_window_update(option, 1);

  /* Initial window size to 64KiB - 1*/
  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  nghttp2_option_del(option);

  stream = open_sent_stream(session, 1);

  /* Create DATA frame */
  memset(data, 0, sizeof(data));
  nghttp2_frame_hd_init(&hd, 357, NGHTTP2_DATA, NGHTTP2_FLAG_PADDED, 1);

  nghttp2_frame_pack_frame_hd(data, &hd);
  /* Set Pad Length field, which itself is padding */
  data[NGHTTP2_FRAME_HDLEN] = 255;

  assert_ptrdiff(
    (nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + hd.length), ==,
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + hd.length));

  assert_int32((int32_t)hd.length, ==, session->recv_window_size);
  assert_int32((int32_t)hd.length, ==, stream->recv_window_size);
  assert_int32(256, ==, session->consumed_size);
  assert_int32(256, ==, stream->consumed_size);
  assert_int32(357, ==, session->recv_window_size);
  assert_int32(357, ==, stream->recv_window_size);

  /* Receive the same DATA frame, but in 2 parts: first 9 + 1 + 102
     bytes which includes 1st padding byte, and remainder */
  assert_ptrdiff(
    (nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + 103), ==,
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 103));
  assert_int32(258, ==, session->consumed_size);
  assert_int32(258, ==, stream->consumed_size);
  assert_int32(460, ==, session->recv_window_size);
  assert_int32(460, ==, stream->recv_window_size);

  /* 357 - 103 = 254 bytes left */
  assert_ptrdiff(254, ==, nghttp2_session_mem_recv2(session, data, 254));
  assert_int32(512, ==, session->consumed_size);
  assert_int32(512, ==, stream->consumed_size);
  assert_int32(714, ==, session->recv_window_size);
  assert_int32(714, ==, stream->recv_window_size);

  /* Receive the same DATA frame, but in 2 parts: first 9 = 1 + 101
     bytes which only includes data without padding, 2nd part is
     padding only */
  assert_ptrdiff(
    (nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + 102), ==,
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 102));
  assert_int32(513, ==, session->consumed_size);
  assert_int32(513, ==, stream->consumed_size);
  assert_int32(816, ==, session->recv_window_size);
  assert_int32(816, ==, stream->recv_window_size);

  /* 357 - 102 = 255 bytes left */
  assert_ptrdiff(255, ==, nghttp2_session_mem_recv2(session, data, 255));
  assert_int32(768, ==, session->consumed_size);
  assert_int32(768, ==, stream->consumed_size);
  assert_int32(1071, ==, session->recv_window_size);
  assert_int32(1071, ==, stream->recv_window_size);

  /* Receive the same DATA frame, but in 2 parts: first 9 = 1 + 50
     bytes which includes byte up to middle of data, 2nd part is the
     remainder */
  assert_ptrdiff(
    (nghttp2_ssize)(NGHTTP2_FRAME_HDLEN + 51), ==,
    nghttp2_session_mem_recv2(session, data, NGHTTP2_FRAME_HDLEN + 51));
  assert_int32(769, ==, session->consumed_size);
  assert_int32(769, ==, stream->consumed_size);
  assert_int32(1122, ==, session->recv_window_size);
  assert_int32(1122, ==, stream->recv_window_size);

  /* 357 - 51 = 306 bytes left */
  assert_ptrdiff(306, ==, nghttp2_session_mem_recv2(session, data, 306));
  assert_int32(1024, ==, session->consumed_size);
  assert_int32(1024, ==, stream->consumed_size);
  assert_int32(1428, ==, session->recv_window_size);
  assert_int32(1428, ==, stream->recv_window_size);

  nghttp2_session_del(session);
}

void test_nghttp2_session_data_read_temporal_failure(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_data_provider2 data_prd;
  nghttp2_frame frame;
  nghttp2_stream *stream;
  size_t data_size = 128 * 1024;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  data_prd.read_callback = fixed_length_data_source_read_callback;

  ud.data_source_length = data_size;

  /* Initial window size is 64KiB - 1 */
  nghttp2_session_client_new(&session, &callbacks, &ud);
  nghttp2_submit_request2(session, NULL, NULL, 0, &data_prd, NULL);

  /* Sends NGHTTP2_INITIAL_WINDOW_SIZE data, assuming, it is equal to
     or smaller than NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(data_size - NGHTTP2_INITIAL_WINDOW_SIZE, ==,
              ud.data_source_length);

  stream = nghttp2_session_get_stream(session, 1);
  assert_uint8(NGHTTP2_DATA, ==, stream->item->frame.hd.type);

  stream->item->aux_data.data.dpw.data_prd.v1.read_callback =
    temporal_failure_data_source_read_callback;

  /* Back NGHTTP2_INITIAL_WINDOW_SIZE to both connection-level and
     stream-wise window */
  nghttp2_frame_window_update_init(&frame.window_update, NGHTTP2_FLAG_NONE, 1,
                                   NGHTTP2_INITIAL_WINDOW_SIZE);
  nghttp2_session_on_window_update_received(session, &frame);
  frame.hd.stream_id = 0;
  nghttp2_session_on_window_update_received(session, &frame);
  nghttp2_frame_window_update_free(&frame.window_update);

  /* Sending data will fail (soft fail) and treated as stream error */
  ud.frame_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(data_size - NGHTTP2_INITIAL_WINDOW_SIZE, ==,
              ud.data_source_length);

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_RST_STREAM, ==, ud.sent_frame_type);

  data_prd.read_callback = fail_data_source_read_callback;
  nghttp2_submit_request2(session, NULL, NULL, 0, &data_prd, NULL);
  /* Sending data will fail (hard fail) and session tear down */
  assert_int(NGHTTP2_ERR_CALLBACK_FAILURE, ==, nghttp2_session_send(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_stream_close(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_stream_close_callback = on_stream_close_callback;
  user_data.stream_close_cb_called = 0;

  nghttp2_session_client_new(&session, &callbacks, &user_data);
  stream = open_sent_stream3(session, 1, NGHTTP2_STREAM_FLAG_NONE,
                             NGHTTP2_STREAM_OPENED, &user_data);
  assert_not_null(stream);
  assert_int(0, ==, nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR));
  assert_int(1, ==, user_data.stream_close_cb_called);
  nghttp2_session_del(session);
}

void test_nghttp2_session_on_ctrl_not_send(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data user_data;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;
  callbacks.send_callback2 = null_send_callback;
  user_data.frame_not_send_cb_called = 0;
  user_data.not_sent_frame_type = 0;
  user_data.not_sent_error = 0;

  nghttp2_session_server_new(&session, &callbacks, &user_data);
  stream = open_recv_stream3(session, 1, NGHTTP2_STREAM_FLAG_NONE,
                             NGHTTP2_STREAM_OPENING, &user_data);

  /* Check response HEADERS */
  /* Send bogus stream ID */
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 3, NULL,
                                      NULL, 0, NULL));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, user_data.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, user_data.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_STREAM_CLOSED, ==, user_data.not_sent_error);

  user_data.frame_not_send_cb_called = 0;
  /* Shutdown transmission */
  stream->shut_flags |= NGHTTP2_SHUT_WR;
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      NULL, 0, NULL));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, user_data.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, user_data.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_STREAM_SHUT_WR, ==, user_data.not_sent_error);

  stream->shut_flags = NGHTTP2_SHUT_NONE;
  user_data.frame_not_send_cb_called = 0;
  /* Queue RST_STREAM */
  assert_int32(0, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, 1, NULL,
                                      NULL, 0, NULL));
  assert_int(0, ==,
             nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 1,
                                       NGHTTP2_INTERNAL_ERROR));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, user_data.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, user_data.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_STREAM_CLOSING, ==, user_data.not_sent_error);

  nghttp2_session_del(session);

  /* Check request HEADERS */
  user_data.frame_not_send_cb_called = 0;
  assert_int(0, ==,
             nghttp2_session_client_new(&session, &callbacks, &user_data));
  /* Maximum Stream ID is reached */
  session->next_stream_id = (1u << 31) + 1;
  assert_int32(NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE, ==,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, -1,
                                      NULL, NULL, 0, NULL));

  user_data.frame_not_send_cb_called = 0;
  /* GOAWAY received */
  session->goaway_flags |= NGHTTP2_GOAWAY_RECV;
  session->next_stream_id = 9;

  assert_int32(0, <,
               nghttp2_submit_headers(session, NGHTTP2_FLAG_END_STREAM, -1,
                                      NULL, NULL, 0, NULL));
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, user_data.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, user_data.not_sent_frame_type);
  assert_int(NGHTTP2_ERR_START_STREAM_NOT_ALLOWED, ==,
             user_data.not_sent_error);

  nghttp2_session_del(session);
}

void test_nghttp2_session_get_outbound_queue_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, NULL));
  assert_size(0, ==, nghttp2_session_get_outbound_queue_size(session));

  assert_int(0, ==, nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL));
  assert_size(1, ==, nghttp2_session_get_outbound_queue_size(session));

  assert_int(0, ==,
             nghttp2_submit_goaway(session, NGHTTP2_FLAG_NONE, 2,
                                   NGHTTP2_NO_ERROR, NULL, 0));
  assert_size(2, ==, nghttp2_session_get_outbound_queue_size(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_get_effective_local_window_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, NULL));

  stream = open_sent_stream(session, 1);

  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE, ==,
               nghttp2_session_get_effective_local_window_size(session));
  assert_int32(0, ==, nghttp2_session_get_effective_recv_data_length(session));

  assert_int32(
    NGHTTP2_INITIAL_WINDOW_SIZE, ==,
    nghttp2_session_get_stream_effective_local_window_size(session, 1));
  assert_int32(
    0, ==, nghttp2_session_get_stream_effective_recv_data_length(session, 1));

  /* Check connection flow control */
  session->recv_window_size = 100;
  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0, 1100);

  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_effective_local_window_size(session));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_local_window_size(session));
  assert_int32(0, ==, nghttp2_session_get_effective_recv_data_length(session));

  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0, -50);
  /* Now session->recv_window_size = -50 */
  assert_int32(-50, ==, session->recv_window_size);
  assert_int32(50, ==, session->recv_reduction);
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 950, ==,
               nghttp2_session_get_effective_local_window_size(session));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_local_window_size(session));
  assert_int32(0, ==, nghttp2_session_get_effective_recv_data_length(session));

  session->recv_window_size += 50;

  /* Now session->recv_window_size = 0 */

  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 950, ==,
               nghttp2_session_get_local_window_size(session));

  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 0, 100);
  assert_int32(50, ==, session->recv_window_size);
  assert_int32(0, ==, session->recv_reduction);
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1050, ==,
               nghttp2_session_get_effective_local_window_size(session));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_local_window_size(session));
  assert_int32(50, ==, nghttp2_session_get_effective_recv_data_length(session));

  /* Check stream flow control */
  stream->recv_window_size = 100;
  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 1, 1100);

  assert_int32(
    NGHTTP2_INITIAL_WINDOW_SIZE + 1000, ==,
    nghttp2_session_get_stream_effective_local_window_size(session, 1));
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));
  assert_int32(
    0, ==, nghttp2_session_get_stream_effective_recv_data_length(session, 1));

  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 1, -50);
  /* Now stream->recv_window_size = -50 */
  assert_int32(
    NGHTTP2_INITIAL_WINDOW_SIZE + 950, ==,
    nghttp2_session_get_stream_effective_local_window_size(session, 1));
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));
  assert_int32(
    0, ==, nghttp2_session_get_stream_effective_recv_data_length(session, 1));

  stream->recv_window_size += 50;
  /* Now stream->recv_window_size = 0 */
  nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, 1, 100);
  assert_int32(
    NGHTTP2_INITIAL_WINDOW_SIZE + 1050, ==,
    nghttp2_session_get_stream_effective_local_window_size(session, 1));
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE + 1000, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));
  assert_int32(
    50, ==, nghttp2_session_get_stream_effective_recv_data_length(session, 1));

  nghttp2_session_del(session);
}

void test_nghttp2_session_set_option(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_option *option;
  nghttp2_hd_deflater *deflater;
  int rv;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  /* Test for nghttp2_option_set_no_auto_window_update */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_window_update(option, 1);

  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  assert_true(session->opt_flags & NGHTTP2_OPTMASK_NO_AUTO_WINDOW_UPDATE);

  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* Test for nghttp2_option_set_peer_max_concurrent_streams */
  nghttp2_option_new(&option);
  nghttp2_option_set_peer_max_concurrent_streams(option, 100);

  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  assert_uint32(100, ==, session->remote_settings.max_concurrent_streams);
  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* Test for nghttp2_option_set_max_reserved_remote_streams */
  nghttp2_option_new(&option);
  nghttp2_option_set_max_reserved_remote_streams(option, 99);

  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  assert_size(99, ==, session->max_incoming_reserved_streams);
  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* Test for nghttp2_option_set_no_auto_ping_ack */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_auto_ping_ack(option, 1);

  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  assert_true(session->opt_flags & NGHTTP2_OPTMASK_NO_AUTO_PING_ACK);

  nghttp2_session_del(session);
  nghttp2_option_del(option);

  /* Test for nghttp2_option_set_max_deflate_dynamic_table_size */
  nghttp2_option_new(&option);
  nghttp2_option_set_max_deflate_dynamic_table_size(option, 0);

  nghttp2_session_client_new2(&session, &callbacks, NULL, option);

  deflater = &session->hd_deflater;

  rv = nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int(1, ==, rv);

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);
  assert_size(0, ==, deflater->deflate_hd_table_bufsize_max);
  assert_size(0, ==, deflater->ctx.hd_table_bufsize);

  nghttp2_session_del(session);
  nghttp2_option_del(option);
}

void test_nghttp2_session_data_backoff_by_high_pri_frame(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_data_provider2 data_prd;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = block_count_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  data_prd.read_callback = fixed_length_data_source_read_callback;

  ud.frame_send_cb_called = 0;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 4;

  nghttp2_session_client_new(&session, &callbacks, &ud);
  nghttp2_submit_request2(session, NULL, NULL, 0, &data_prd, NULL);

  session->remote_window_size = 1 << 20;

  ud.block_count = 2;
  /* Sends request HEADERS + DATA[0] */
  assert_int(0, ==, nghttp2_session_send(session));

  stream = nghttp2_session_get_stream(session, 1);
  stream->remote_window_size = 1 << 20;

  assert_uint8(NGHTTP2_DATA, ==, ud.sent_frame_type);
  /* data for DATA[1] is read from data_prd but it is not sent */
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN * 2);

  nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL);
  ud.block_count = 2;
  /* Sends DATA[1] + PING, PING is interleaved in DATA sequence */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_PING, ==, ud.sent_frame_type);
  /* data for DATA[2] is read from data_prd but it is not sent */
  assert_size(ud.data_source_length, ==, NGHTTP2_DATA_PAYLOADLEN);

  ud.block_count = 2;
  /* Sends DATA[2..3] */
  assert_int(0, ==, nghttp2_session_send(session));

  assert_true(stream->shut_flags & NGHTTP2_SHUT_WR);

  nghttp2_session_del(session);
}

static void check_session_recv_data_with_padding(nghttp2_bufs *bufs,
                                                 size_t datalen,
                                                 nghttp2_mem *mem) {
  nghttp2_session *session;
  my_user_data ud;
  nghttp2_session_callbacks callbacks;
  uint8_t *in;
  size_t inlen;

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_data_chunk_recv_callback = on_data_chunk_recv_callback;
  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 1);

  inlen = (size_t)nghttp2_bufs_remove(bufs, &in);

  ud.frame_recv_cb_called = 0;
  ud.data_chunk_len = 0;

  assert_ptrdiff((nghttp2_ssize)inlen, ==,
                 nghttp2_session_mem_recv2(session, in, inlen));

  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_size(datalen, ==, ud.data_chunk_len);

  mem->free(in, NULL);
  nghttp2_session_del(session);
}

void test_nghttp2_session_pack_data_with_padding(void) {
  nghttp2_session *session;
  my_user_data ud;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  nghttp2_frame *frame;
  size_t datalen = 55;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback2 = block_count_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.select_padding_callback2 = select_padding_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  ud.padlen = 63;

  nghttp2_submit_request2(session, NULL, NULL, 0, &data_prd, NULL);
  ud.block_count = 1;
  ud.data_source_length = datalen;
  /* Sends HEADERS */
  assert_int(0, ==, nghttp2_session_send(session));
  assert_uint8(NGHTTP2_HEADERS, ==, ud.sent_frame_type);

  frame = &session->aob.item->frame;

  assert_size(ud.padlen, ==, frame->data.padlen);
  assert_true(frame->hd.flags & NGHTTP2_FLAG_PADDED);

  /* Check reception of this DATA frame */
  check_session_recv_data_with_padding(&session->aob.framebufs, datalen, mem);

  nghttp2_session_del(session);
}

void test_nghttp2_session_pack_headers_with_padding(void) {
  nghttp2_session *session, *sv_session;
  accumulator acc;
  my_user_data ud;
  nghttp2_session_callbacks callbacks;

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.select_padding_callback2 = select_padding_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;

  acc.length = 0;
  ud.acc = &acc;

  nghttp2_session_client_new(&session, &callbacks, &ud);
  nghttp2_session_server_new(&sv_session, &callbacks, &ud);

  ud.padlen = 163;

  assert_int32(
    1, ==,
    nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL));
  assert_int(0, ==, nghttp2_session_send(session));

  assert_size(NGHTTP2_MAX_PAYLOADLEN, >, acc.length);
  ud.frame_recv_cb_called = 0;
  assert_ptrdiff((nghttp2_ssize)acc.length, ==,
                 nghttp2_session_mem_recv2(sv_session, acc.buf, acc.length));
  assert_int(1, ==, ud.frame_recv_cb_called);
  assert_null(nghttp2_session_get_next_ob_item(sv_session));

  nghttp2_session_del(sv_session);
  nghttp2_session_del(session);
}

void test_nghttp2_pack_settings_payload(void) {
  nghttp2_settings_entry iv[2];
  uint8_t buf[64];
  nghttp2_ssize len;
  nghttp2_settings_entry *resiv;
  size_t resniv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 1023;
  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 4095;

  len = nghttp2_pack_settings_payload2(buf, sizeof(buf), iv, 2);
  assert_ptrdiff(2 * NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH, ==, len);
  assert_int(0, ==,
             nghttp2_frame_unpack_settings_payload2(&resiv, &resniv, buf,
                                                    (size_t)len, mem));
  assert_size(2, ==, resniv);
  assert_int32(NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, ==, resiv[0].settings_id);
  assert_uint32(1023, ==, resiv[0].value);
  assert_int32(NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, ==, resiv[1].settings_id);
  assert_uint32(4095, ==, resiv[1].value);

  mem->free(resiv, NULL);

  len = nghttp2_pack_settings_payload2(buf, 9 /* too small */, iv, 2);
  assert_ptrdiff(NGHTTP2_ERR_INSUFF_BUFSIZE, ==, len);
}

void test_nghttp2_session_stream_get_state(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_mem *mem;
  nghttp2_hd_deflater deflater;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_stream *stream;
  nghttp2_ssize rv;
  nghttp2_data_provider2 data_prd;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);
  memset(&data_prd, 0, sizeof(data_prd));

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);
  nghttp2_hd_deflate_init(&deflater, mem);

  assert_enum(
    nghttp2_stream_proto_state, NGHTTP2_STREAM_STATE_IDLE, ==,
    nghttp2_stream_get_state(nghttp2_session_get_root_stream(session)));

  /* stream 1 HEADERS; without END_STREAM flag set */
  pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, reqnv,
               ARRLEN(reqnv), mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  stream = nghttp2_session_find_stream(session, 1);

  assert_not_null(stream);
  assert_int32(1, ==, stream->stream_id);
  assert_enum(nghttp2_stream_proto_state, NGHTTP2_STREAM_STATE_OPEN, ==,
              nghttp2_stream_get_state(stream));

  nghttp2_bufs_reset(&bufs);

  /* stream 3 HEADERS; with END_STREAM flag set */
  pack_headers(&bufs, &deflater, 3,
               NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, reqnv,
               ARRLEN(reqnv), mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  stream = nghttp2_session_find_stream(session, 3);

  assert_not_null(stream);
  assert_int32(3, ==, stream->stream_id);
  assert_enum(nghttp2_stream_proto_state,
              NGHTTP2_STREAM_STATE_HALF_CLOSED_REMOTE, ==,
              nghttp2_stream_get_state(stream));

  nghttp2_bufs_reset(&bufs);

  /* Respond to stream 1 */
  nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), NULL);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 1);

  assert_enum(nghttp2_stream_proto_state,
              NGHTTP2_STREAM_STATE_HALF_CLOSED_LOCAL, ==,
              nghttp2_stream_get_state(stream));

  /* Respond to stream 3 */
  nghttp2_submit_response2(session, 3, resnv, ARRLEN(resnv), NULL);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 3);

  assert_null(stream);

  /* stream 5 HEADERS; with END_STREAM flag set */
  pack_headers(&bufs, &deflater, 5,
               NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, reqnv,
               ARRLEN(reqnv), mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  nghttp2_bufs_reset(&bufs);

  /* Push stream 2 associated to stream 5 */
  rv = nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 5, reqnv,
                                   ARRLEN(reqnv), NULL);

  assert_ptrdiff(2, ==, rv);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 2);

  assert_enum(nghttp2_stream_proto_state, NGHTTP2_STREAM_STATE_RESERVED_LOCAL,
              ==, nghttp2_stream_get_state(stream));

  /* Send response to push stream 2 with END_STREAM set */
  nghttp2_submit_response2(session, 2, resnv, ARRLEN(resnv), NULL);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 2);

  /* At server, pushed stream object is not retained after closed */
  assert_null(stream);

  /* Push stream 4 associated to stream 5 */
  rv = nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 5, reqnv,
                                   ARRLEN(reqnv), NULL);

  assert_ptrdiff(4, ==, rv);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 4);

  assert_enum(nghttp2_stream_proto_state, NGHTTP2_STREAM_STATE_RESERVED_LOCAL,
              ==, nghttp2_stream_get_state(stream));

  /* Send response to push stream 4 without closing */
  data_prd.read_callback = defer_data_source_read_callback;

  nghttp2_submit_response2(session, 4, resnv, ARRLEN(resnv), &data_prd);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_find_stream(session, 4);

  assert_enum(nghttp2_stream_proto_state,
              NGHTTP2_STREAM_STATE_HALF_CLOSED_REMOTE, ==,
              nghttp2_stream_get_state(stream));

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Test for client side */

  nghttp2_session_client_new(&session, &callbacks, NULL);
  nghttp2_hd_deflate_init(&deflater, mem);

  nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  /* Receive PUSH_PROMISE 2 associated to stream 1 */
  pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 2, reqnv,
                    ARRLEN(reqnv), mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  stream = nghttp2_session_find_stream(session, 2);

  assert_enum(nghttp2_stream_proto_state, NGHTTP2_STREAM_STATE_RESERVED_REMOTE,
              ==, nghttp2_stream_get_state(stream));

  nghttp2_bufs_reset(&bufs);

  /* Receive push response for stream 2 without END_STREAM set */
  pack_headers(&bufs, &deflater, 2, NGHTTP2_FLAG_END_HEADERS, resnv,
               ARRLEN(resnv), mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  stream = nghttp2_session_find_stream(session, 2);

  assert_enum(nghttp2_stream_proto_state,
              NGHTTP2_STREAM_STATE_HALF_CLOSED_LOCAL, ==,
              nghttp2_stream_get_state(stream));

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_find_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));

  nghttp2_session_server_new(&session, &callbacks, NULL);

  open_recv_stream(session, 1);

  stream = nghttp2_session_find_stream(session, 1);

  assert_not_null(stream);
  assert_int32(1, ==, stream->stream_id);

  stream = nghttp2_session_find_stream(session, 0);

  assert_not_null(stream);
  assert_int32(0, ==, stream->stream_id);

  stream = nghttp2_session_find_stream(session, 2);

  assert_null(stream);

  nghttp2_session_del(session);
}

void test_nghttp2_session_graceful_shutdown(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.on_stream_close_callback = on_stream_close_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 301);
  open_sent_stream(session, 302);
  open_recv_stream(session, 309);
  open_recv_stream(session, 311);
  open_recv_stream(session, 319);

  assert_int(0, ==, nghttp2_submit_shutdown_notice(session));

  ud.frame_send_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_int32((1u << 31) - 1, ==, session->local_last_stream_id);

  assert_int(0, ==,
             nghttp2_submit_goaway(session, NGHTTP2_FLAG_NONE, 311,
                                   NGHTTP2_NO_ERROR, NULL, 0));

  ud.frame_send_cb_called = 0;
  ud.stream_close_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_int32(311, ==, session->local_last_stream_id);
  assert_int(1, ==, ud.stream_close_cb_called);

  assert_int(
    0, ==, nghttp2_session_terminate_session2(session, 301, NGHTTP2_NO_ERROR));

  ud.frame_send_cb_called = 0;
  ud.stream_close_cb_called = 0;

  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_send_cb_called);
  assert_int32(301, ==, session->local_last_stream_id);
  assert_int(2, ==, ud.stream_close_cb_called);

  assert_not_null(nghttp2_session_get_stream(session, 301));
  assert_not_null(nghttp2_session_get_stream(session, 302));
  assert_null(nghttp2_session_get_stream(session, 309));
  assert_null(nghttp2_session_get_stream(session, 311));
  assert_null(nghttp2_session_get_stream(session, 319));

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_header_temporal_failure(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_hd_deflater deflater;
  nghttp2_nv nv[] = {MAKE_NV("alpha", "bravo"), MAKE_NV("charlie", "delta")};
  nghttp2_nv *nva;
  size_t hdpos;
  nghttp2_ssize rv;
  nghttp2_frame frame;
  nghttp2_frame_hd hd;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_header_callback = temporal_failure_on_header_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  frame_pack_bufs_init(&bufs);

  nghttp2_hd_deflate_init(&deflater, mem);

  nghttp2_nv_array_copy(&nva, reqnv, ARRLEN(reqnv), mem);

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_STREAM, 1,
                             NGHTTP2_HCAT_REQUEST, NULL, nva, ARRLEN(reqnv));
  nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);
  nghttp2_frame_headers_free(&frame.headers, mem);

  /* We are going to create CONTINUATION.  First serialize header
     block, and then frame header. */
  hdpos = nghttp2_bufs_len(&bufs);

  buf = &bufs.head->buf;
  buf->last += NGHTTP2_FRAME_HDLEN;

  nghttp2_hd_deflate_hd_bufs(&deflater, &bufs, &nv[1], 1);

  nghttp2_frame_hd_init(&hd,
                        nghttp2_bufs_len(&bufs) - hdpos - NGHTTP2_FRAME_HDLEN,
                        NGHTTP2_CONTINUATION, NGHTTP2_FLAG_END_HEADERS, 1);

  nghttp2_frame_pack_frame_hd(&buf->pos[hdpos], &hd);

  ud.header_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.header_cb_called);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.hd.stream_id);

  /* Make sure no header decompression error occurred */
  assert_uint8(NGHTTP2_GOAWAY_NONE, ==, session->goaway_flags);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_bufs_reset(&bufs);

  /* Check for PUSH_PROMISE */
  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_session_client_new(&session, &callbacks, &ud);

  open_sent_stream(session, 1);

  rv = pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 2,
                         reqnv, ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  ud.header_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));
  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(1, ==, ud.header_cb_called);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(2, ==, item->frame.hd.stream_id);
  assert_uint32(NGHTTP2_INTERNAL_ERROR, ==, item->frame.rst_stream.error_code);

  nghttp2_session_del(session);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_recv_client_magic(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_ssize rv;
  nghttp2_frame ping_frame;
  uint8_t buf[16];

  /* enable global nghttp2_enable_strict_preface here */
  nghttp2_enable_strict_preface = 1;

  memset(&callbacks, 0, sizeof(callbacks));

  /* Check success case */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  rv = nghttp2_session_mem_recv2(session, (const uint8_t *)NGHTTP2_CLIENT_MAGIC,
                                 NGHTTP2_CLIENT_MAGIC_LEN);

  assert_ptrdiff(NGHTTP2_CLIENT_MAGIC_LEN, ==, rv);
  assert_enum(nghttp2_inbound_state, NGHTTP2_IB_READ_FIRST_SETTINGS, ==,
              session->iframe.state);

  /* Receiving PING is error because we want SETTINGS. */
  nghttp2_frame_ping_init(&ping_frame.ping, NGHTTP2_FLAG_NONE, NULL);

  nghttp2_frame_pack_frame_hd(buf, &ping_frame.ping.hd);

  rv = nghttp2_session_mem_recv2(session, buf, NGHTTP2_FRAME_HDLEN);
  assert_ptrdiff(NGHTTP2_FRAME_HDLEN, ==, rv);
  assert_enum(nghttp2_inbound_state, NGHTTP2_IB_IGN_ALL, ==,
              session->iframe.state);
  assert_size(0, ==, session->iframe.payloadleft);

  nghttp2_frame_ping_free(&ping_frame.ping);

  nghttp2_session_del(session);

  /* Check bad case */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  /* Feed magic with one byte less */
  rv = nghttp2_session_mem_recv2(session, (const uint8_t *)NGHTTP2_CLIENT_MAGIC,
                                 NGHTTP2_CLIENT_MAGIC_LEN - 1);

  assert_ptrdiff(NGHTTP2_CLIENT_MAGIC_LEN - 1, ==, rv);
  assert_enum(nghttp2_inbound_state, NGHTTP2_IB_READ_CLIENT_MAGIC, ==,
              session->iframe.state);
  assert_size(1, ==, session->iframe.payloadleft);

  rv = nghttp2_session_mem_recv2(session, (const uint8_t *)"\0", 1);

  assert_ptrdiff(NGHTTP2_ERR_BAD_CLIENT_MAGIC, ==, rv);

  nghttp2_session_del(session);

  /* disable global nghttp2_enable_strict_preface here */
  nghttp2_enable_strict_preface = 0;
}

void test_nghttp2_session_delete_data_item(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 prd;

  memset(&callbacks, 0, sizeof(callbacks));

  nghttp2_session_server_new(&session, &callbacks, NULL);

  open_recv_stream(session, 1);
  open_recv_stream(session, 3);

  /* We don't care about these members, since we won't send data */
  prd.source.ptr = NULL;
  prd.read_callback = fail_data_source_read_callback;

  assert_int(0, ==, nghttp2_submit_data2(session, NGHTTP2_FLAG_NONE, 1, &prd));
  assert_int(0, ==, nghttp2_submit_data2(session, NGHTTP2_FLAG_NONE, 3, &prd));

  nghttp2_session_del(session);
}

void test_nghttp2_session_open_idle_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;
  nghttp2_stream *opened_stream;
  nghttp2_frame frame;
  nghttp2_ext_priority_update priority_update;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_server_new(&session, &callbacks, NULL);

  frame.ext.payload = &priority_update;

  nghttp2_frame_priority_update_init(&frame.ext, 1, (uint8_t *)"u=3",
                                     strlen("u=3"));

  assert_int(0, ==,
             nghttp2_session_on_priority_update_received(session, &frame));

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_IDLE, ==, stream->state);
  assert_null(stream->closed_next);
  assert_size(1, ==, session->num_idle_streams);

  opened_stream = open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  assert_ptr_equal(stream, opened_stream);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);
  assert_size(0, ==, session->num_idle_streams);

  nghttp2_frame_priority_free(&frame.priority);

  nghttp2_session_del(session);

  /* No RFC 7540 priorities */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  session->pending_no_rfc7540_priorities = 1;

  frame.ext.payload = &priority_update;

  nghttp2_frame_priority_update_init(&frame.ext, 1, (uint8_t *)"u=3",
                                     strlen("u=3"));

  assert_int(0, ==,
             nghttp2_session_on_priority_update_received(session, &frame));

  stream = nghttp2_session_get_stream_raw(session, 1);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_IDLE, ==, stream->state);
  assert_null(stream->closed_next);
  assert_size(1, ==, session->num_idle_streams);

  opened_stream = open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  assert_ptr_equal(stream, opened_stream);
  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_OPENING, ==, stream->state);
  assert_size(0, ==, session->num_idle_streams);

  nghttp2_frame_priority_free(&frame.priority);

  nghttp2_session_del(session);
}

void test_nghttp2_session_cancel_reserved_remote(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;
  nghttp2_frame frame;
  nghttp2_nv *nva;
  size_t nvlen;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  stream = open_recv_stream2(session, 2, NGHTTP2_STREAM_RESERVED);

  nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 2, NGHTTP2_CANCEL);

  assert_enum(nghttp2_stream_state, NGHTTP2_STREAM_CLOSING, ==, stream->state);

  assert_int(0, ==, nghttp2_session_send(session));

  nvlen = ARRLEN(resnv);
  nghttp2_nv_array_copy(&nva, resnv, nvlen, mem);

  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS, 2,
                             NGHTTP2_HCAT_PUSH_RESPONSE, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  /* stream is not dangling, so assign NULL */
  stream = NULL;

  /* No RST_STREAM or GOAWAY is generated since stream should be in
     NGHTTP2_STREAM_CLOSING and push response should be ignored. */
  assert_size(0, ==, nghttp2_outbound_queue_size(&session->ob_reg));

  /* Check that we can receive push response HEADERS while RST_STREAM
     is just queued. */
  open_recv_stream2(session, 4, NGHTTP2_STREAM_RESERVED);

  nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 2, NGHTTP2_CANCEL);

  nghttp2_bufs_reset(&bufs);

  frame.hd.stream_id = 4;
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_size(1, ==, nghttp2_outbound_queue_size(&session->ob_reg));

  nghttp2_frame_headers_free(&frame.headers, mem);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_reset_pending_headers(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_stream *stream;
  int32_t stream_id;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;
  callbacks.on_stream_close_callback = on_stream_close_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  stream_id = nghttp2_submit_request2(session, NULL, NULL, 0, NULL, NULL);
  assert_int32(1, <=, stream_id);

  nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                            NGHTTP2_CANCEL);

  session->remote_settings.max_concurrent_streams = 0;

  /* RST_STREAM cancels pending HEADERS and is not actually sent. */
  ud.frame_send_cb_called = 0;
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(0, ==, ud.frame_send_cb_called);

  stream = nghttp2_session_get_stream(session, stream_id);

  assert_null(stream);

  /* See HEADERS is not sent.  on_stream_close is called just like
     transmission failure. */
  session->remote_settings.max_concurrent_streams = 1;

  ud.frame_not_send_cb_called = 0;
  ud.stream_close_error_code = 0;
  assert_int(0, ==, nghttp2_session_send(session));

  assert_int(1, ==, ud.frame_not_send_cb_called);
  assert_uint8(NGHTTP2_HEADERS, ==, ud.not_sent_frame_type);
  assert_uint32(NGHTTP2_CANCEL, ==, ud.stream_close_error_code);

  stream = nghttp2_session_get_stream(session, stream_id);

  assert_null(stream);

  nghttp2_session_del(session);
}

void test_nghttp2_session_send_data_callback(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  accumulator acc;
  nghttp2_frame_hd hd;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = accumulator_send_callback;
  callbacks.send_data_callback = send_data_callback;

  data_prd.read_callback = no_copy_data_source_read_callback;

  acc.length = 0;
  ud.acc = &acc;

  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN * 2;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  open_sent_stream(session, 1);

  nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd);

  assert_int(0, ==, nghttp2_session_send(session));

  assert_size((NGHTTP2_FRAME_HDLEN + NGHTTP2_DATA_PAYLOADLEN) * 2, ==,
              acc.length);

  nghttp2_frame_unpack_frame_hd(&hd, acc.buf);

  assert_size(16384, ==, hd.length);
  assert_uint8(NGHTTP2_DATA, ==, hd.type);
  assert_uint8(NGHTTP2_FLAG_NONE, ==, hd.flags);

  nghttp2_frame_unpack_frame_hd(&hd, acc.buf + NGHTTP2_FRAME_HDLEN + hd.length);

  assert_size(16384, ==, hd.length);
  assert_uint8(NGHTTP2_DATA, ==, hd.type);
  assert_uint8(NGHTTP2_FLAG_END_STREAM, ==, hd.flags);

  nghttp2_session_del(session);
}

void test_nghttp2_session_on_begin_headers_temporal_failure(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;
  nghttp2_ssize rv;
  nghttp2_hd_deflater deflater;
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);
  nghttp2_hd_deflate_init(&deflater, mem);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_begin_headers_callback =
    temporal_failure_on_begin_headers_callback;
  callbacks.on_header_callback = on_header_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.send_callback2 = null_send_callback;
  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  ud.header_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));
  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.header_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.hd.stream_id);
  assert_uint32(NGHTTP2_INTERNAL_ERROR, ==, item->frame.rst_stream.error_code);

  nghttp2_session_del(session);
  nghttp2_hd_deflate_free(&deflater);

  nghttp2_bufs_reset(&bufs);
  /* check for PUSH_PROMISE */
  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_session_client_new(&session, &callbacks, &ud);

  open_sent_stream(session, 1);

  rv = pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 2,
                         reqnv, ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  ud.header_cb_called = 0;
  ud.frame_recv_cb_called = 0;
  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_bufs_len(&bufs));
  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, rv);
  assert_int(0, ==, ud.header_cb_called);
  assert_int(0, ==, ud.frame_recv_cb_called);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(2, ==, item->frame.hd.stream_id);
  assert_uint32(NGHTTP2_INTERNAL_ERROR, ==, item->frame.rst_stream.error_code);

  nghttp2_session_del(session);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_defer_then_close(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 prd;
  int rv;
  const uint8_t *datap;
  nghttp2_ssize datalen;
  nghttp2_frame frame;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  prd.read_callback = defer_data_source_read_callback;

  rv = nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), &prd, NULL);
  assert_ptrdiff(0, <, rv);

  /* This sends HEADERS */
  datalen = nghttp2_session_mem_send2(session, &datap);

  assert_ptrdiff(0, <, datalen);

  /* This makes DATA item deferred */
  datalen = nghttp2_session_mem_send2(session, &datap);

  assert_ptrdiff(0, ==, datalen);

  nghttp2_frame_rst_stream_init(&frame.rst_stream, 1, NGHTTP2_CANCEL);

  /* Assertion failure; GH-264 */
  rv = nghttp2_session_on_rst_stream_received(session, &frame);

  assert_int(0, ==, rv);

  nghttp2_session_del(session);
}

static int submit_response_on_stream_close(nghttp2_session *session,
                                           int32_t stream_id,
                                           uint32_t error_code,
                                           void *user_data) {
  nghttp2_data_provider2 data_prd;
  (void)error_code;
  (void)user_data;

  data_prd.read_callback = temporal_failure_data_source_read_callback;

  // Attempt to submit response or data to the stream being closed
  switch (stream_id) {
  case 1:
    assert_int(0, ==,
               nghttp2_submit_response2(session, stream_id, resnv,
                                        ARRLEN(resnv), &data_prd));
    break;
  case 3:
    assert_int(
      0, ==,
      nghttp2_submit_data2(session, NGHTTP2_FLAG_NONE, stream_id, &data_prd));
    break;
  }

  return 0;
}

void test_nghttp2_session_detach_item_from_closed_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;

  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.send_callback2 = null_send_callback;
  callbacks.on_stream_close_callback = submit_response_on_stream_close;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  open_recv_stream(session, 1);
  open_recv_stream(session, 3);

  nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR);
  nghttp2_session_close_stream(session, 3, NGHTTP2_NO_ERROR);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_session_del(session);

  /* No RFC 7540 priorities */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  session->pending_no_rfc7540_priorities = 1;

  open_recv_stream(session, 1);
  open_recv_stream(session, 3);

  nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR);
  nghttp2_session_close_stream(session, 3, NGHTTP2_NO_ERROR);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_flooding(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_frame frame;
  nghttp2_mem *mem;
  size_t i;

  mem = nghttp2_mem_default();

  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(callbacks));

  /* PING ACK */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_frame_ping_init(&frame.ping, NGHTTP2_FLAG_NONE, NULL);
  nghttp2_frame_pack_ping(&bufs, &frame.ping);
  nghttp2_frame_ping_free(&frame.ping);

  buf = &bufs.head->buf;

  for (i = 0; i < NGHTTP2_DEFAULT_MAX_OBQ_FLOOD_ITEM; ++i) {
    assert_ptrdiff(
      (nghttp2_ssize)nghttp2_buf_len(buf), ==,
      nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf)));
  }

  assert_ptrdiff(
    NGHTTP2_ERR_FLOODED, ==,
    nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf)));

  nghttp2_session_del(session);

  /* SETTINGS ACK */
  nghttp2_bufs_reset(&bufs);

  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, NULL, 0);
  nghttp2_frame_pack_settings(&bufs, &frame.settings);
  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;

  for (i = 0; i < NGHTTP2_DEFAULT_MAX_OBQ_FLOOD_ITEM; ++i) {
    assert_ptrdiff(
      (nghttp2_ssize)nghttp2_buf_len(buf), ==,
      nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf)));
  }

  assert_ptrdiff(
    NGHTTP2_ERR_FLOODED, ==,
    nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf)));

  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_change_extpri_stream_priority(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  nghttp2_option *option;
  nghttp2_extension frame;
  nghttp2_ext_priority_update priority_update;
  nghttp2_extpri extpri, nextpri;
  nghttp2_stream *stream;
  static const uint8_t field_value[] = "u=2";

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  frame_pack_bufs_init(&bufs);

  nghttp2_option_new(&option);
  nghttp2_option_set_builtin_recv_extension_type(option,
                                                 NGHTTP2_PRIORITY_UPDATE);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  session->pending_no_rfc7540_priorities = 1;

  open_recv_stream(session, 1);

  extpri.urgency = NGHTTP2_EXTPRI_URGENCY_LOW + 1;
  extpri.inc = 1;

  rv = nghttp2_session_change_extpri_stream_priority(
    session, 1, &extpri, /* ignore_client_signal = */ 0);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_get_stream(session, 1);

  assert_uint32(NGHTTP2_EXTPRI_URGENCY_LOW, ==,
                nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  rv = nghttp2_session_get_extpri_stream_priority(session, &nextpri, 1);

  assert_ptrdiff(0, ==, rv);
  assert_uint32(NGHTTP2_EXTPRI_URGENCY_LOW, ==, nextpri.urgency);
  assert_true(nextpri.inc);

  /* Client can still update stream priority. */
  frame.payload = &priority_update;
  nghttp2_frame_priority_update_init(&frame, 1, (uint8_t *)field_value,
                                     sizeof(field_value) - 1);
  nghttp2_frame_pack_priority_update(&bufs, &frame);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_uint8(2, ==, stream->extpri);

  /* Start to ignore client priority signal for this stream. */
  rv = nghttp2_session_change_extpri_stream_priority(
    session, 1, &extpri, /* ignore_client_signal = */ 1);

  assert_ptrdiff(0, ==, rv);

  stream = nghttp2_session_get_stream(session, 1);

  assert_uint32(NGHTTP2_EXTPRI_URGENCY_LOW, ==,
                nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);
  assert_uint32(NGHTTP2_EXTPRI_URGENCY_LOW, ==,
                nghttp2_extpri_uint8_urgency(stream->extpri));
  assert_true(nghttp2_extpri_uint8_inc(stream->extpri));

  nghttp2_session_del(session);
  nghttp2_option_del(option);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_set_local_window_size(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_outbound_item *item;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);
  stream = open_sent_stream(session, 1);
  stream->recv_window_size = 4096;

  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   1, 65536));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1, ==,
               stream->local_window_size);
  assert_int32(4096, ==, stream->recv_window_size);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.window_update.hd.stream_id);
  assert_int32(1, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Go decrement part */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   1, 32768));
  assert_int32(32768, ==, stream->local_window_size);
  assert_int32(-28672, ==, stream->recv_window_size);
  assert_int32(32768, ==, stream->recv_reduction);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  /* Increase local window size */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   1, 49152));
  assert_int32(49152, ==, stream->local_window_size);
  assert_int32(-12288, ==, stream->recv_window_size);
  assert_int32(16384, ==, stream->recv_reduction);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));
  assert_null(nghttp2_session_get_next_ob_item(session));

  /* Increase local window again */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   1, 65537));
  assert_int32(65537, ==, stream->local_window_size);
  assert_int32(4096, ==, stream->recv_window_size);
  assert_int32(0, ==, stream->recv_reduction);
  assert_int32(65537 - 4096, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));

  item = nghttp2_session_get_next_ob_item(session);

  assert_int32(1, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Check connection-level flow control */
  session->recv_window_size = 4096;
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   0, 65536));
  assert_int32(NGHTTP2_INITIAL_CONNECTION_WINDOW_SIZE + 1, ==,
               session->local_window_size);
  assert_int32(4096, ==, session->recv_window_size);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_local_window_size(session));

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.window_update.hd.stream_id);
  assert_int32(1, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  /* Go decrement part */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   0, 32768));
  assert_int32(32768, ==, session->local_window_size);
  assert_int32(-28672, ==, session->recv_window_size);
  assert_int32(32768, ==, session->recv_reduction);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_local_window_size(session));

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  /* Increase local window size */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   0, 49152));
  assert_int32(49152, ==, session->local_window_size);
  assert_int32(-12288, ==, session->recv_window_size);
  assert_int32(16384, ==, session->recv_reduction);
  assert_int32(65536 - 4096, ==,
               nghttp2_session_get_local_window_size(session));
  assert_null(nghttp2_session_get_next_ob_item(session));

  /* Increase local window again */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE,
                                                   0, 65537));
  assert_int32(65537, ==, session->local_window_size);
  assert_int32(4096, ==, session->recv_window_size);
  assert_int32(0, ==, session->recv_reduction);
  assert_int32(65537 - 4096, ==,
               nghttp2_session_get_local_window_size(session));

  item = nghttp2_session_get_next_ob_item(session);

  assert_int32(1, ==, item->frame.window_update.window_size_increment);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_session_del(session);

  /* Make sure that nghttp2_session_set_local_window_size submits
     WINDOW_UPDATE if necessary to increase stream-level window. */
  nghttp2_session_client_new(&session, &callbacks, NULL);
  stream = open_sent_stream(session, 1);
  stream->recv_window_size = NGHTTP2_INITIAL_WINDOW_SIZE;

  assert_int(
    0, ==,
    nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE, 1, 0));
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(0, ==, nghttp2_session_get_stream_local_window_size(session, 1));
  /* This should submit WINDOW_UPDATE frame because stream-level
     receiving window is now full. */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(
               session, NGHTTP2_FLAG_NONE, 1, NGHTTP2_INITIAL_WINDOW_SIZE));
  assert_int32(0, ==, stream->recv_window_size);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
               nghttp2_session_get_stream_local_window_size(session, 1));

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.hd.stream_id);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
               item->frame.window_update.window_size_increment);

  nghttp2_session_del(session);

  /* Make sure that nghttp2_session_set_local_window_size submits
     WINDOW_UPDATE if necessary to increase connection-level
     window. */
  nghttp2_session_client_new(&session, &callbacks, NULL);
  session->recv_window_size = NGHTTP2_INITIAL_WINDOW_SIZE;

  assert_int(
    0, ==,
    nghttp2_session_set_local_window_size(session, NGHTTP2_FLAG_NONE, 0, 0));
  assert_int32(0, ==, session->recv_window_size);
  assert_int32(0, ==, nghttp2_session_get_local_window_size(session));
  /* This should submit WINDOW_UPDATE frame because connection-level
     receiving window is now full. */
  assert_int(0, ==,
             nghttp2_session_set_local_window_size(
               session, NGHTTP2_FLAG_NONE, 0, NGHTTP2_INITIAL_WINDOW_SIZE));
  assert_int32(0, ==, session->recv_window_size);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
               nghttp2_session_get_local_window_size(session));

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_WINDOW_UPDATE, ==, item->frame.hd.type);
  assert_int32(0, ==, item->frame.hd.stream_id);
  assert_int32(NGHTTP2_INITIAL_WINDOW_SIZE, ==,
               item->frame.window_update.window_size_increment);

  nghttp2_session_del(session);
}

void test_nghttp2_session_cancel_from_before_frame_send(void) {
  int rv;
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  my_user_data ud;
  nghttp2_settings_entry iv;
  nghttp2_data_provider2 data_prd;
  int32_t stream_id;
  nghttp2_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.before_frame_send_callback = cancel_before_frame_send_callback;
  callbacks.on_frame_not_send_callback = on_frame_not_send_callback;
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  iv.settings_id = 0;
  iv.value = 1000000009;

  rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1);

  assert_int(0, ==, rv);

  ud.frame_send_cb_called = 0;
  ud.before_frame_send_cb_called = 0;
  ud.frame_not_send_cb_called = 0;

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_send_cb_called);
  assert_int(1, ==, ud.before_frame_send_cb_called);
  assert_int(1, ==, ud.frame_not_send_cb_called);

  data_prd.source.ptr = NULL;
  data_prd.read_callback = temporal_failure_data_source_read_callback;

  stream_id = nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv),
                                      &data_prd, NULL);

  assert_int32(0, <, stream_id);

  ud.frame_send_cb_called = 0;
  ud.before_frame_send_cb_called = 0;
  ud.frame_not_send_cb_called = 0;

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_send_cb_called);
  assert_int(1, ==, ud.before_frame_send_cb_called);
  assert_int(1, ==, ud.frame_not_send_cb_called);

  stream = nghttp2_session_get_stream_raw(session, stream_id);

  assert_null(stream);

  nghttp2_session_del(session);

  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 1);

  stream_id = nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE, 1, reqnv,
                                          ARRLEN(reqnv), NULL);

  assert_int32(0, <, stream_id);

  ud.frame_send_cb_called = 0;
  ud.before_frame_send_cb_called = 0;
  ud.frame_not_send_cb_called = 0;

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);
  assert_int(0, ==, ud.frame_send_cb_called);
  assert_int(1, ==, ud.before_frame_send_cb_called);
  assert_int(1, ==, ud.frame_not_send_cb_called);

  stream = nghttp2_session_get_stream_raw(session, stream_id);

  assert_null(stream);

  nghttp2_session_del(session);
}

void test_nghttp2_session_too_many_settings(void) {
  nghttp2_session *session;
  nghttp2_option *option;
  nghttp2_session_callbacks callbacks;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_ssize rv;
  my_user_data ud;
  nghttp2_settings_entry iv[3];
  nghttp2_mem *mem;
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.send_callback2 = null_send_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_max_settings(option, 1);

  nghttp2_session_client_new2(&session, &callbacks, &ud, option);

  assert_size(1, ==, session->max_settings);

  nghttp2_option_del(option);

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 3000;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = 16384;

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, dup_iv(iv, 2),
                              2);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  assert(nghttp2_bufs_len(&bufs) == nghttp2_buf_len(buf));

  ud.frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));
  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);

  nghttp2_bufs_reset(&bufs);
  nghttp2_bufs_free(&bufs);
  nghttp2_session_del(session);
}

static void
prepare_session_removed_closed_stream(nghttp2_session *session,
                                      nghttp2_hd_deflater *deflater) {
  int rv;
  nghttp2_settings_entry iv;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;
  nghttp2_ssize nread;
  int i;
  nghttp2_stream *stream;
  nghttp2_frame_hd hd;

  mem = nghttp2_mem_default();

  frame_pack_bufs_init(&bufs);

  iv.settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv.value = 2;

  rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1);

  assert_int(0, ==, rv);

  rv = nghttp2_session_send(session);

  assert_int(0, ==, rv);

  for (i = 1; i <= 3; i += 2) {
    rv = pack_headers(&bufs, deflater, i,
                      NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, reqnv,
                      ARRLEN(reqnv), mem);

    assert_int(0, ==, rv);

    nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                      nghttp2_bufs_len(&bufs));

    assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);

    nghttp2_bufs_reset(&bufs);
  }

  nghttp2_session_close_stream(session, 3, NGHTTP2_NO_ERROR);

  rv = pack_headers(&bufs, deflater, 5,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, reqnv,
                    ARRLEN(reqnv), mem);

  assert_int(0, ==, rv);

  /* Receiving stream 5 will erase stream 3 from closed stream list */
  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);

  stream = nghttp2_session_get_stream_raw(session, 3);

  assert_null(stream);

  /* Since the current max concurrent streams is
     NGHTTP2_DEFAULT_MAX_CONCURRENT_STREAMS, receiving frame on stream
     3 is ignored. */
  nghttp2_bufs_reset(&bufs);
  rv = pack_headers(&bufs, deflater, 3,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    trailernv, ARRLEN(trailernv), mem);

  assert_int(0, ==, rv);

  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);
  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 3);
  nghttp2_bufs_reset(&bufs);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);
  assert_null(nghttp2_session_get_next_ob_item(session));

  /* Now server receives SETTINGS ACK */
  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_SETTINGS, NGHTTP2_FLAG_ACK, 0);
  nghttp2_bufs_reset(&bufs);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_session_removed_closed_stream(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  int rv;
  nghttp2_hd_deflater deflater;
  nghttp2_bufs bufs;
  nghttp2_mem *mem;
  nghttp2_ssize nread;
  nghttp2_frame_hd hd;
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();

  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  /* Now local max concurrent streams is still unlimited, pending max
     concurrent streams is now 2. */

  nghttp2_hd_deflate_init(&deflater, mem);

  prepare_session_removed_closed_stream(session, &deflater);

  /* Now current max concurrent streams is 2.  Receiving frame on
     stream 3 is ignored because we have no stream object for stream
     3. */
  nghttp2_bufs_reset(&bufs);
  rv = pack_headers(&bufs, &deflater, 3,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    trailernv, ARRLEN(trailernv), mem);

  assert_int(0, ==, rv);

  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_session_server_new(&session, &callbacks, NULL);
  nghttp2_hd_deflate_init(&deflater, mem);
  /* Same setup, and then receive DATA instead of HEADERS */

  prepare_session_removed_closed_stream(session, &deflater);

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 3);
  nghttp2_bufs_reset(&bufs);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  nread = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                    nghttp2_bufs_len(&bufs));

  assert_ptrdiff((nghttp2_ssize)nghttp2_bufs_len(&bufs), ==, nread);

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

static nghttp2_ssize pause_once_data_source_read_callback(
  nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t len,
  uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
  my_user_data *ud = user_data;
  if (ud->data_source_read_cb_paused == 0) {
    ++ud->data_source_read_cb_paused;
    return NGHTTP2_ERR_PAUSE;
  }

  return fixed_length_data_source_read_callback(session, stream_id, buf, len,
                                                data_flags, source, user_data);
}

void test_nghttp2_session_pause_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_send_callback = on_frame_send_callback;

  data_prd.read_callback = pause_once_data_source_read_callback;
  ud.data_source_length = NGHTTP2_DATA_PAYLOADLEN;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  open_recv_stream(session, 1);

  assert_int(
    0, ==,
    nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 1, &data_prd));

  ud.frame_send_cb_called = 0;
  ud.data_source_read_cb_paused = 0;

  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(0, ==, ud.frame_send_cb_called);
  assert_null(session->aob.item);
  assert_int(0, ==, nghttp2_session_send(session));
  assert_int(1, ==, ud.frame_send_cb_called);
  assert_uint8(NGHTTP2_DATA, ==, ud.sent_frame_type);
  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_session_del(session);
}

void test_nghttp2_session_no_closed_streams(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_option *option;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_option_new(&option);
  nghttp2_option_set_no_closed_streams(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  open_recv_stream(session, 1);

  nghttp2_session_close_stream(session, 1, NGHTTP2_NO_ERROR);

  assert_size(0, ==, session->num_closed_streams);

  nghttp2_session_del(session);
  nghttp2_option_del(option);
}

void test_nghttp2_session_set_stream_user_data(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  int32_t stream_id;
  int user_data1, user_data2;
  int rv;
  const uint8_t *datap;
  nghttp2_ssize datalen;

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));

  nghttp2_session_client_new(&session, &callbacks, NULL);

  stream_id = nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL,
                                      &user_data1);

  rv = nghttp2_session_set_stream_user_data(session, stream_id, &user_data2);

  assert_int(0, ==, rv);

  datalen = nghttp2_session_mem_send2(session, &datap);

  assert_ptrdiff(0, <, datalen);

  assert_ptr_equal(&user_data2,
                   nghttp2_session_get_stream_user_data(session, stream_id));

  assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==,
             nghttp2_session_set_stream_user_data(session, 2, NULL));

  nghttp2_session_del(session);
}

void test_nghttp2_session_no_rfc7540_priorities(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_data_provider2 data_prd;
  my_user_data ud;
  nghttp2_outbound_item *item;
  nghttp2_mem *mem;
  nghttp2_settings_entry iv;
  nghttp2_priority_spec pri_spec;

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  /* Do not use a dependency tree if SETTINGS_NO_RFC7540_PRIORITIES =
     1. */
  data_prd.read_callback = fixed_length_data_source_read_callback;

  ud.data_source_length = 128 * 1024;
  assert_int(0, ==, nghttp2_session_server_new(&session, &callbacks, &ud));

  iv.settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv.value = 1;

  assert_int(0, ==,
             nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1));
  assert_int(0, ==, nghttp2_session_send(session));

  open_recv_stream2(session, 1, NGHTTP2_STREAM_OPENING);
  assert_int(
    0, ==,
    nghttp2_submit_response2(session, 1, resnv, ARRLEN(resnv), &data_prd));
  item = nghttp2_session_get_next_ob_item(session);
  assert_size(ARRLEN(resnv), ==, item->frame.headers.nvlen);
  assert_nv_equal(resnv, item->frame.headers.nva, item->frame.headers.nvlen,
                  mem);

  assert_int(0, ==, nghttp2_session_send(session));
  assert_size(
    1, ==,
    nghttp2_pq_size(&session->sched[NGHTTP2_EXTPRI_DEFAULT_URGENCY].ob_data));

  nghttp2_session_del(session);

  /* Priorities are defaulted */
  assert_int(0, ==, nghttp2_session_client_new(&session, &callbacks, NULL));

  iv.settings_id = NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES;
  iv.value = 1;

  assert_int(0, ==,
             nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &iv, 1));

  session->remote_settings.no_rfc7540_priorities = 1;

  pri_spec.stream_id = 5;
  pri_spec.weight = 111;
  pri_spec.exclusive = 1;

  assert_int32(1, ==,
               nghttp2_submit_request2(session, &pri_spec, reqnv, ARRLEN(reqnv),
                                       NULL, NULL));

  item = nghttp2_outbound_queue_top(&session->ob_syn);

  assert_not_null(item);
  assert_uint8(NGHTTP2_HEADERS, ==, item->frame.hd.type);
  assert_true(
    nghttp2_priority_spec_check_default(&item->frame.headers.pri_spec));

  nghttp2_session_del(session);
}

void test_nghttp2_session_stream_reset_ratelim(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_frame frame;
  nghttp2_ssize rv;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_mem *mem;
  size_t i;
  nghttp2_hd_deflater deflater;
  size_t nvlen;
  nghttp2_nv *nva;
  int32_t stream_id;
  nghttp2_outbound_item *item;
  nghttp2_option *option;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_option_new(&option);
  nghttp2_option_set_stream_reset_rate_limit(
    option, NGHTTP2_DEFAULT_STREAM_RESET_BURST, 0);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, NULL, 0);
  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_settings_free(&frame.settings, mem);

  buf = &bufs.head->buf;
  rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

  /* Send SETTINGS ACK */
  rv = nghttp2_session_send(session);

  assert_ptrdiff(0, ==, rv);

  nghttp2_hd_deflate_init(&deflater, mem);

  for (i = 0; i < NGHTTP2_DEFAULT_STREAM_RESET_BURST + 2; ++i) {
    stream_id = (int32_t)(i * 2 + 1);

    nghttp2_bufs_reset(&bufs);

    /* HEADERS */
    nvlen = ARRLEN(reqnv);
    nghttp2_nv_array_copy(&nva, reqnv, nvlen, mem);
    nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_HEADERS,
                               stream_id, NGHTTP2_HCAT_HEADERS, NULL, nva,
                               nvlen);
    rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);

    assert_ptrdiff(0, ==, rv);

    nghttp2_frame_headers_free(&frame.headers, mem);

    buf = &bufs.head->buf;
    rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

    assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

    nghttp2_bufs_reset(&bufs);

    /* RST_STREAM */
    nghttp2_frame_rst_stream_init(&frame.rst_stream, stream_id,
                                  NGHTTP2_NO_ERROR);
    nghttp2_frame_pack_rst_stream(&bufs, &frame.rst_stream);
    nghttp2_frame_rst_stream_free(&frame.rst_stream);

    buf = &bufs.head->buf;
    rv = nghttp2_session_mem_recv2(session, buf->pos, nghttp2_buf_len(buf));

    assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(buf), ==, rv);

    if (i < NGHTTP2_DEFAULT_STREAM_RESET_BURST) {
      assert_size(0, ==, nghttp2_outbound_queue_size(&session->ob_reg));

      continue;
    }

    assert_size(1, ==, nghttp2_outbound_queue_size(&session->ob_reg));

    item = nghttp2_session_get_next_ob_item(session);

    assert_uint8(NGHTTP2_GOAWAY, ==, item->frame.hd.type);
    assert_int32(NGHTTP2_DEFAULT_STREAM_RESET_BURST * 2 + 1, ==,
                 item->frame.goaway.last_stream_id);
  }

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
  nghttp2_option_del(option);
}

static void check_nghttp2_http_recv_headers_fail(
  nghttp2_session *session, nghttp2_hd_deflater *deflater, int32_t stream_id,
  int stream_state, const nghttp2_nv *nva, size_t nvlen) {
  nghttp2_mem *mem;
  nghttp2_ssize rv;
  nghttp2_outbound_item *item;
  nghttp2_bufs bufs;
  my_user_data *ud;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  ud = session->user_data;

  if (stream_state != -1) {
    if (nghttp2_session_is_my_stream_id(session, stream_id)) {
      open_sent_stream2(session, stream_id, (nghttp2_stream_state)stream_state);
    } else {
      open_recv_stream2(session, stream_id, (nghttp2_stream_state)stream_state);
    }
  }

  rv = pack_headers(&bufs, deflater, stream_id, NGHTTP2_FLAG_END_HEADERS, nva,
                    nvlen, mem);
  assert_ptrdiff(0, ==, rv);

  ud->invalid_frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_ptrdiff(1, ==, ud->invalid_frame_recv_cb_called);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_free(&bufs);
}

static void check_nghttp2_http_recv_headers_ok(
  nghttp2_session *session, nghttp2_hd_deflater *deflater, int32_t stream_id,
  int stream_state, const nghttp2_nv *nva, size_t nvlen) {
  nghttp2_mem *mem;
  nghttp2_ssize rv;
  nghttp2_bufs bufs;
  my_user_data *ud;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  ud = session->user_data;

  if (stream_state != -1) {
    if (nghttp2_session_is_my_stream_id(session, stream_id)) {
      open_sent_stream2(session, stream_id, (nghttp2_stream_state)stream_state);
    } else {
      open_recv_stream2(session, stream_id, (nghttp2_stream_state)stream_state);
    }
  }

  rv = pack_headers(&bufs, deflater, stream_id, NGHTTP2_FLAG_END_HEADERS, nva,
                    nvlen, mem);
  assert_ptrdiff(0, ==, rv);

  ud->frame_recv_cb_called = 0;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);
  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_int(1, ==, ud->frame_recv_cb_called);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_mandatory_headers(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  my_user_data ud;
  /* test case for response */
  const nghttp2_nv nostatus_resnv[] = {MAKE_NV("server", "foo")};
  const nghttp2_nv dupstatus_resnv[] = {MAKE_NV(":status", "200"),
                                        MAKE_NV(":status", "200")};
  const nghttp2_nv badpseudo_resnv[] = {MAKE_NV(":status", "200"),
                                        MAKE_NV(":scheme", "https")};
  const nghttp2_nv latepseudo_resnv[] = {MAKE_NV("server", "foo"),
                                         MAKE_NV(":status", "200")};
  const nghttp2_nv badstatus_resnv[] = {MAKE_NV(":status", "2000")};
  const nghttp2_nv badcl_resnv[] = {MAKE_NV(":status", "200"),
                                    MAKE_NV("content-length", "-1")};
  const nghttp2_nv dupcl_resnv[] = {MAKE_NV(":status", "200"),
                                    MAKE_NV("content-length", "0"),
                                    MAKE_NV("content-length", "0")};
  const nghttp2_nv badhd_resnv[] = {MAKE_NV(":status", "200"),
                                    MAKE_NV("connection", "close")};
  const nghttp2_nv cl1xx_resnv[] = {MAKE_NV(":status", "100"),
                                    MAKE_NV("content-length", "0")};
  const nghttp2_nv cl204_resnv[] = {MAKE_NV(":status", "204"),
                                    MAKE_NV("content-length", "0")};
  const nghttp2_nv clnonzero204_resnv[] = {MAKE_NV(":status", "204"),
                                           MAKE_NV("content-length", "100")};
  const nghttp2_nv status101_resnv[] = {MAKE_NV(":status", "101")};
  const nghttp2_nv unexpectedhost_resnv[] = {MAKE_NV(":status", "200"),
                                             MAKE_NV("host", "/localhost")};

  /* test case for request */
  const nghttp2_nv nopath_reqnv[] = {MAKE_NV(":scheme", "https"),
                                     MAKE_NV(":method", "GET"),
                                     MAKE_NV(":authority", "localhost")};
  const nghttp2_nv earlyconnect_reqnv[] = {
    MAKE_NV(":method", "CONNECT"), MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"), MAKE_NV(":authority", "localhost")};
  const nghttp2_nv lateconnect_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "CONNECT"), MAKE_NV(":authority", "localhost")};
  const nghttp2_nv duppath_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
    MAKE_NV(":path", "/")};
  const nghttp2_nv badcl_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
    MAKE_NV("content-length", "-1")};
  const nghttp2_nv dupcl_reqnv[] = {
    MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
    MAKE_NV("content-length", "0"),     MAKE_NV("content-length", "0")};
  const nghttp2_nv badhd_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
    MAKE_NV("connection", "close")};
  const nghttp2_nv badauthority_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "\x0d\x0alocalhost"), MAKE_NV(":path", "/")};
  const nghttp2_nv badhdbtw_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "GET"),
    MAKE_NV("foo", "\x0d\x0a"), MAKE_NV(":authority", "localhost"),
    MAKE_NV(":path", "/")};
  const nghttp2_nv asteriskget1_reqnv[] = {
    MAKE_NV(":path", "*"), MAKE_NV(":scheme", "https"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":method", "GET")};
  const nghttp2_nv asteriskget2_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "localhost"),
    MAKE_NV(":method", "GET"), MAKE_NV(":path", "*")};
  const nghttp2_nv asteriskoptions1_reqnv[] = {
    MAKE_NV(":path", "*"), MAKE_NV(":scheme", "https"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":method", "OPTIONS")};
  const nghttp2_nv asteriskoptions2_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "localhost"),
    MAKE_NV(":method", "OPTIONS"), MAKE_NV(":path", "*")};
  const nghttp2_nv connectproto_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "CONNECT"), MAKE_NV(":authority", "localhost"),
    MAKE_NV(":protocol", "websocket")};
  const nghttp2_nv connectprotoget_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"), MAKE_NV(":authority", "localhost"),
    MAKE_NV(":protocol", "websocket")};
  const nghttp2_nv connectprotonopath_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":method", "CONNECT"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":protocol", "websocket")};
  const nghttp2_nv connectprotonoauth_reqnv[] = {
    MAKE_NV(":scheme", "http"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "CONNECT"), MAKE_NV("host", "localhost"),
    MAKE_NV(":protocol", "websocket")};
  const nghttp2_nv regularconnect_reqnv[] = {
    MAKE_NV(":method", "CONNECT"), MAKE_NV(":authority", "localhost")};

  mem = nghttp2_mem_default();

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_frame_recv_callback = on_frame_recv_callback;
  callbacks.on_invalid_frame_recv_callback = on_invalid_frame_recv_callback;

  nghttp2_session_client_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* response header lacks :status */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 1,
                                       NGHTTP2_STREAM_OPENING, nostatus_resnv,
                                       ARRLEN(nostatus_resnv));

  /* response header has 2 :status */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 3,
                                       NGHTTP2_STREAM_OPENING, dupstatus_resnv,
                                       ARRLEN(dupstatus_resnv));

  /* response header has bad pseudo header :scheme */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 5,
                                       NGHTTP2_STREAM_OPENING, badpseudo_resnv,
                                       ARRLEN(badpseudo_resnv));

  /* response header has :status after regular header field */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 7,
                                       NGHTTP2_STREAM_OPENING, latepseudo_resnv,
                                       ARRLEN(latepseudo_resnv));

  /* response header has bad status code */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 9,
                                       NGHTTP2_STREAM_OPENING, badstatus_resnv,
                                       ARRLEN(badstatus_resnv));

  /* response header has bad content-length */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 11,
                                       NGHTTP2_STREAM_OPENING, badcl_resnv,
                                       ARRLEN(badcl_resnv));

  /* response header has multiple content-length */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 13,
                                       NGHTTP2_STREAM_OPENING, dupcl_resnv,
                                       ARRLEN(dupcl_resnv));

  /* response header has disallowed header field */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 15,
                                       NGHTTP2_STREAM_OPENING, badhd_resnv,
                                       ARRLEN(badhd_resnv));

  /* response header has content-length with 100 status code */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 17,
                                       NGHTTP2_STREAM_OPENING, cl1xx_resnv,
                                       ARRLEN(cl1xx_resnv));

  /* response header has 0 content-length with 204 status code */
  check_nghttp2_http_recv_headers_ok(session, &deflater, 19,
                                     NGHTTP2_STREAM_OPENING, cl204_resnv,
                                     ARRLEN(cl204_resnv));

  /* response header has nonzero content-length with 204 status
     code */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 21, NGHTTP2_STREAM_OPENING, clnonzero204_resnv,
    ARRLEN(clnonzero204_resnv));

  /* status code 101 should not be used in HTTP/2 because it is used
     for HTTP Upgrade which HTTP/2 removes. */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 23,
                                       NGHTTP2_STREAM_OPENING, status101_resnv,
                                       ARRLEN(status101_resnv));

  /* Specific characters check for host field in response header
     should not be done as its use is undefined. */
  check_nghttp2_http_recv_headers_ok(
    session, &deflater, 25, NGHTTP2_STREAM_OPENING, unexpectedhost_resnv,
    ARRLEN(unexpectedhost_resnv));

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  /* check server side */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* request header has no :path */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 1, -1, nopath_reqnv,
                                       ARRLEN(nopath_reqnv));

  /* request header has CONNECT method, but followed by :path */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 3, -1, earlyconnect_reqnv, ARRLEN(earlyconnect_reqnv));

  /* request header has CONNECT method following :path */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 5, -1, lateconnect_reqnv, ARRLEN(lateconnect_reqnv));

  /* request header has multiple :path */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 7, -1, duppath_reqnv,
                                       ARRLEN(duppath_reqnv));

  /* request header has bad content-length */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 9, -1, badcl_reqnv,
                                       ARRLEN(badcl_reqnv));

  /* request header has multiple content-length */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 11, -1, dupcl_reqnv,
                                       ARRLEN(dupcl_reqnv));

  /* request header has disallowed header field */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 13, -1, badhd_reqnv,
                                       ARRLEN(badhd_reqnv));

  /* request header has :authority header field containing illegal
     characters */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 15, -1, badauthority_reqnv, ARRLEN(badauthority_reqnv));

  /* request header has regular header field containing illegal
     character before all mandatory header fields are seen. */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 17, -1,
                                       badhdbtw_reqnv, ARRLEN(badhdbtw_reqnv));

  /* request header has "*" in :path header field while method is GET.
     :path is received before :method */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 19, -1, asteriskget1_reqnv, ARRLEN(asteriskget1_reqnv));

  /* request header has "*" in :path header field while method is GET.
     :method is received before :path */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 21, -1, asteriskget2_reqnv, ARRLEN(asteriskget2_reqnv));

  /* OPTIONS method can include "*" in :path header field.  :path is
     received before :method. */
  check_nghttp2_http_recv_headers_ok(session, &deflater, 23, -1,
                                     asteriskoptions1_reqnv,
                                     ARRLEN(asteriskoptions1_reqnv));

  /* OPTIONS method can include "*" in :path header field.  :method is
     received before :path. */
  check_nghttp2_http_recv_headers_ok(session, &deflater, 25, -1,
                                     asteriskoptions2_reqnv,
                                     ARRLEN(asteriskoptions2_reqnv));

  /* :protocol is not allowed unless it is enabled by the local
     endpoint. */
  check_nghttp2_http_recv_headers_fail(
    session, &deflater, 27, -1, connectproto_reqnv, ARRLEN(connectproto_reqnv));

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  /* enable SETTINGS_CONNECT_PROTOCOL */
  nghttp2_session_server_new(&session, &callbacks, &ud);

  session->pending_enable_connect_protocol = 1;

  nghttp2_hd_deflate_init(&deflater, mem);

  /* :protocol is allowed if SETTINGS_CONNECT_PROTOCOL is enabled by
     the local endpoint. */
  check_nghttp2_http_recv_headers_ok(
    session, &deflater, 1, -1, connectproto_reqnv, ARRLEN(connectproto_reqnv));

  /* :protocol is only allowed with CONNECT method. */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 3, -1,
                                       connectprotoget_reqnv,
                                       ARRLEN(connectprotoget_reqnv));

  /* CONNECT method with :protocol requires :path. */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 5, -1,
                                       connectprotonopath_reqnv,
                                       ARRLEN(connectprotonopath_reqnv));

  /* CONNECT method with :protocol requires :authority. */
  check_nghttp2_http_recv_headers_fail(session, &deflater, 7, -1,
                                       connectprotonoauth_reqnv,
                                       ARRLEN(connectprotonoauth_reqnv));

  /* regular CONNECT method should succeed with
     SETTINGS_CONNECT_PROTOCOL */
  check_nghttp2_http_recv_headers_ok(session, &deflater, 9, -1,
                                     regularconnect_reqnv,
                                     ARRLEN(regularconnect_reqnv));

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);
}

void test_nghttp2_http_content_length(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  nghttp2_stream *stream;
  const nghttp2_nv cl_resnv[] = {MAKE_NV(":status", "200"),
                                 MAKE_NV("te", "trailers"),
                                 MAKE_NV("content-length", "9000000000")};
  const nghttp2_nv cl_reqnv[] = {
    MAKE_NV(":path", "/"),        MAKE_NV(":method", "PUT"),
    MAKE_NV(":scheme", "https"),  MAKE_NV("te", "trailers"),
    MAKE_NV("host", "localhost"), MAKE_NV("content-length", "9000000000")};

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  stream = open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, cl_resnv,
                    ARRLEN(cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);
  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_int64(9000000000LL, ==, stream->content_length);
  assert_int16(200, ==, stream->status_code);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  nghttp2_bufs_reset(&bufs);

  /* check server side */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, cl_reqnv,
                    ARRLEN(cl_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  stream = nghttp2_session_get_stream(session, 1);

  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_int64(9000000000LL, ==, stream->content_length);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_content_length_mismatch(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  const nghttp2_nv cl_reqnv[] = {
    MAKE_NV(":path", "/"), MAKE_NV(":method", "PUT"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":scheme", "https"),
    MAKE_NV("content-length", "20")};
  const nghttp2_nv cl_resnv[] = {MAKE_NV(":status", "200"),
                                 MAKE_NV("content-length", "20")};
  nghttp2_outbound_item *item;
  nghttp2_frame_hd hd;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* header says content-length: 20, but HEADERS has END_STREAM flag set */
  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    cl_reqnv, ARRLEN(cl_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* header says content-length: 20, but DATA has 0 byte */
  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS, cl_reqnv,
                    ARRLEN(cl_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 3);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* header says content-length: 20, but DATA has 21 bytes */
  rv = pack_headers(&bufs, &deflater, 5, NGHTTP2_FLAG_END_HEADERS, cl_reqnv,
                    ARRLEN(cl_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 21, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 5);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN + 21;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  /* Check for client */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* header says content-length: 20, but HEADERS has END_STREAM flag set */
  nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int(0, ==, nghttp2_session_send(session));

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    cl_resnv, ARRLEN(cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_not_null(nghttp2_session_get_stream(session, 1));
  assert_int(0, ==, nghttp2_session_send(session));
  /* After sending RST_STREAM, stream must be closed */
  assert_null(nghttp2_session_get_stream(session, 1));

  nghttp2_bufs_reset(&bufs);

  /* header says content-length: 20, but DATA has 0 byte */
  nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int(0, ==, nghttp2_session_send(session));

  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS, cl_resnv,
                    ARRLEN(cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 3);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_not_null(nghttp2_session_get_stream(session, 3));
  assert_int(0, ==, nghttp2_session_send(session));
  /* After sending RST_STREAM, stream must be closed */
  assert_null(nghttp2_session_get_stream(session, 3));

  nghttp2_bufs_reset(&bufs);

  /* header says content-length: 20, but DATA has 21 bytes */
  nghttp2_submit_request2(session, NULL, reqnv, ARRLEN(reqnv), NULL, NULL);

  assert_int(0, ==, nghttp2_session_send(session));

  rv = pack_headers(&bufs, &deflater, 5, NGHTTP2_FLAG_END_HEADERS, cl_resnv,
                    ARRLEN(cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 21, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 5);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN + 21;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_not_null(nghttp2_session_get_stream(session, 5));
  assert_int(0, ==, nghttp2_session_send(session));
  /* After sending RST_STREAM, stream must be closed */
  assert_null(nghttp2_session_get_stream(session, 5));

  nghttp2_bufs_reset(&bufs);

  nghttp2_bufs_free(&bufs);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);
}

void test_nghttp2_http_non_final_response(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  const nghttp2_nv nonfinal_resnv[] = {
    MAKE_NV(":status", "100"),
  };
  nghttp2_outbound_item *item;
  nghttp2_frame_hd hd;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* non-final HEADERS with END_STREAM is illegal */
  open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    nonfinal_resnv, ARRLEN(nonfinal_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* non-final HEADERS followed by non-empty DATA is illegal */
  open_sent_stream2(session, 3, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS,
                    nonfinal_resnv, ARRLEN(nonfinal_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 10, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 3);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN + 10;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);
  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* non-final HEADERS followed by empty DATA (without END_STREAM) is
     ok */
  open_sent_stream2(session, 5, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 5, NGHTTP2_FLAG_END_HEADERS,
                    nonfinal_resnv, ARRLEN(nonfinal_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_NONE, 5);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_bufs_reset(&bufs);

  /* non-final HEADERS followed by empty DATA (with END_STREAM) is
     illegal */
  open_sent_stream2(session, 7, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 7, NGHTTP2_FLAG_END_HEADERS,
                    nonfinal_resnv, ARRLEN(nonfinal_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  nghttp2_frame_hd_init(&hd, 0, NGHTTP2_DATA, NGHTTP2_FLAG_END_STREAM, 7);
  nghttp2_frame_pack_frame_hd(bufs.head->buf.last, &hd);
  bufs.head->buf.last += NGHTTP2_FRAME_HDLEN;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* non-final HEADERS followed by final HEADERS is OK */
  open_sent_stream2(session, 9, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 9, NGHTTP2_FLAG_END_HEADERS,
                    nonfinal_resnv, ARRLEN(nonfinal_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  nghttp2_bufs_reset(&bufs);

  rv = pack_headers(&bufs, &deflater, 9, NGHTTP2_FLAG_END_HEADERS, resnv,
                    ARRLEN(resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_trailer_headers(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  const nghttp2_nv trailer_reqnv[] = {
    MAKE_NV("foo", "bar"),
  };
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* good trailer header */
  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  nghttp2_bufs_reset(&bufs);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    trailer_reqnv, ARRLEN(trailer_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_bufs_reset(&bufs);

  /* trailer header without END_STREAM is illegal */
  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  nghttp2_bufs_reset(&bufs);

  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS,
                    trailer_reqnv, ARRLEN(trailer_reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  /* trailer header including pseudo header field is illegal */
  rv = pack_headers(&bufs, &deflater, 5, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  nghttp2_bufs_reset(&bufs);

  rv = pack_headers(&bufs, &deflater, 5, NGHTTP2_FLAG_END_HEADERS, reqnv,
                    ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);

  nghttp2_session_del(session);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_ignore_regular_header(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  my_user_data ud;
  const nghttp2_nv bad_reqnv[] = {
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV("foo", "\x0zzz"),
    MAKE_NV("bar", "buzz"),
  };
  const nghttp2_nv bad_ansnv[] = {
    MAKE_NV(":authority", "localhost"), MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"), MAKE_NV(":method", "GET"), MAKE_NV("bar", "buzz")};
  size_t proclen;
  size_t i;
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;
  callbacks.on_header_callback = pause_on_header_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);
  nghttp2_hd_deflate_init(&deflater, mem);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    bad_reqnv, ARRLEN(bad_reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  nghttp2_hd_deflate_free(&deflater);

  proclen = 0;

  for (i = 0; i < 4; ++i) {
    rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos + proclen,
                                   nghttp2_buf_len(&bufs.head->buf) - proclen);
    assert_ptrdiff(0, <, rv);
    proclen += (size_t)rv;
    assert_true(nghttp2_nv_equal(&bad_ansnv[i], &ud.nv));
  }

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos + proclen,
                                 nghttp2_buf_len(&bufs.head->buf) - proclen);
  assert_ptrdiff(0, <, rv);
  /* Without on_invalid_frame_recv_callback, bad header causes stream
     reset */
  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);

  proclen += (size_t)rv;

  assert_size(nghttp2_buf_len(&bufs.head->buf), ==, proclen);

  nghttp2_session_del(session);

  /* use on_invalid_header_callback */
  callbacks.on_invalid_header_callback = pause_on_invalid_header_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  proclen = 0;

  ud.invalid_header_cb_called = 0;

  for (i = 0; i < 4; ++i) {
    rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos + proclen,
                                   nghttp2_buf_len(&bufs.head->buf) - proclen);
    assert_ptrdiff(0, <, rv);
    proclen += (size_t)rv;
    assert_true(nghttp2_nv_equal(&bad_ansnv[i], &ud.nv));
  }

  assert_int(0, ==, ud.invalid_header_cb_called);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos + proclen,
                                 nghttp2_buf_len(&bufs.head->buf) - proclen);

  assert_ptrdiff(0, <, rv);
  assert_int(1, ==, ud.invalid_header_cb_called);
  assert_true(nghttp2_nv_equal(&bad_reqnv[4], &ud.nv));

  proclen += (size_t)rv;

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos + proclen,
                                 nghttp2_buf_len(&bufs.head->buf) - proclen);

  assert_ptrdiff(0, <, rv);
  assert_true(nghttp2_nv_equal(&bad_ansnv[4], &ud.nv));

  nghttp2_session_del(session);

  /* make sure that we can reset stream from
     on_invalid_header_callback */
  callbacks.on_header_callback = on_header_callback;
  callbacks.on_invalid_header_callback = reset_on_invalid_header_callback;

  nghttp2_session_server_new(&session, &callbacks, &ud);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(1, ==, item->frame.hd.stream_id);

  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_ignore_content_length(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  const nghttp2_nv cl_resnv[] = {MAKE_NV(":status", "304"),
                                 MAKE_NV("content-length", "20")};
  const nghttp2_nv conn_reqnv[] = {MAKE_NV(":authority", "localhost"),
                                   MAKE_NV(":method", "CONNECT"),
                                   MAKE_NV("content-length", "999999")};
  const nghttp2_nv conn_cl_resnv[] = {MAKE_NV(":status", "200"),
                                      MAKE_NV("content-length", "0")};
  nghttp2_stream *stream;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  /* If status 304, content-length must be ignored */
  open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    cl_resnv, ARRLEN(cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  nghttp2_bufs_reset(&bufs);

  /* Content-Length in 200 response to CONNECT is ignored */
  stream = open_sent_stream2(session, 3, NGHTTP2_STREAM_OPENING);
  stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_CONNECT;

  rv = pack_headers(&bufs, &deflater, 3, NGHTTP2_FLAG_END_HEADERS,
                    conn_cl_resnv, ARRLEN(conn_cl_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));
  assert_int64(-1, ==, stream->content_length);

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* If request method is CONNECT, content-length must be ignored */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, conn_reqnv,
                    ARRLEN(conn_reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  stream = nghttp2_session_get_stream(session, 1);

  assert_int64(-1, ==, stream->content_length);
  assert_true(stream->http_flags & NGHTTP2_HTTP_FLAG_METH_CONNECT);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_record_request_method(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  const nghttp2_nv conn_reqnv[] = {MAKE_NV(":method", "CONNECT"),
                                   MAKE_NV(":authority", "localhost")};
  const nghttp2_nv conn_resnv[] = {MAKE_NV(":status", "200"),
                                   MAKE_NV("content-length", "9999")};
  nghttp2_stream *stream;
  nghttp2_ssize rv;
  nghttp2_bufs bufs;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  assert_int32(1, ==,
               nghttp2_submit_request2(session, NULL, conn_reqnv,
                                       ARRLEN(conn_reqnv), NULL, NULL));

  assert_int(0, ==, nghttp2_session_send(session));

  stream = nghttp2_session_get_stream(session, 1);

  assert_uint32(NGHTTP2_HTTP_FLAG_METH_CONNECT, ==, stream->http_flags);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, conn_resnv,
                    ARRLEN(conn_resnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_true(NGHTTP2_HTTP_FLAG_METH_CONNECT & stream->http_flags);
  assert_int64(-1, ==, stream->content_length);

  /* content-length is ignored in 200 response to a CONNECT request */
  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_push_promise(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  nghttp2_stream *stream;
  const nghttp2_nv bad_reqnv[] = {MAKE_NV(":method", "GET")};
  nghttp2_outbound_item *item;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  /* good PUSH_PROMISE case */
  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  open_sent_stream2(session, 1, NGHTTP2_STREAM_OPENING);

  rv = pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 2,
                         reqnv, ARRLEN(reqnv), mem);
  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  stream = nghttp2_session_get_stream(session, 2);
  assert_not_null(stream);

  nghttp2_bufs_reset(&bufs);

  rv = pack_headers(&bufs, &deflater, 2, NGHTTP2_FLAG_END_HEADERS, resnv,
                    ARRLEN(resnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  assert_null(nghttp2_session_get_next_ob_item(session));

  assert_int16(200, ==, stream->status_code);

  nghttp2_bufs_reset(&bufs);

  /* PUSH_PROMISE lacks mandatory header */
  rv = pack_push_promise(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, 4,
                         bad_reqnv, ARRLEN(bad_reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int32(4, ==, item->frame.hd.stream_id);

  nghttp2_bufs_reset(&bufs);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_head_method_upgrade_workaround(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  const nghttp2_nv cl_resnv[] = {MAKE_NV(":status", "200"),
                                 MAKE_NV("content-length", "1000000007")};
  nghttp2_bufs bufs;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_ssize rv;
  nghttp2_stream *stream;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  nghttp2_session_client_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  nghttp2_session_upgrade(session, NULL, 0, NULL);

  rv = pack_headers(&bufs, &deflater, 1, NGHTTP2_FLAG_END_HEADERS, cl_resnv,
                    ARRLEN(cl_resnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  stream = nghttp2_session_get_stream(session, 1);

  assert_int64(-1, ==, stream->content_length);

  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_http_no_rfc9113_leading_and_trailing_ws_validation(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_mem *mem;
  nghttp2_bufs bufs;
  nghttp2_ssize rv;
  const nghttp2_nv ws_reqnv[] = {
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":scheme", "https"),
    MAKE_NV("foo", "bar "),
  };
  nghttp2_outbound_item *item;
  nghttp2_option *option;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  /* By default, the leading and trailing white spaces validation is
     enabled as per RFC 9113. */
  nghttp2_session_server_new(&session, &callbacks, NULL);

  nghttp2_hd_deflate_init(&deflater, mem);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    ws_reqnv, ARRLEN(ws_reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_uint8(NGHTTP2_RST_STREAM, ==, item->frame.hd.type);
  assert_int(0, ==, nghttp2_session_send(session));

  nghttp2_bufs_reset(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);

  /* Turn off the validation */
  nghttp2_option_new(&option);
  nghttp2_option_set_no_rfc9113_leading_and_trailing_ws_validation(option, 1);

  nghttp2_session_server_new2(&session, &callbacks, NULL, option);

  nghttp2_hd_deflate_init(&deflater, mem);

  rv = pack_headers(&bufs, &deflater, 1,
                    NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                    ws_reqnv, ARRLEN(ws_reqnv), mem);

  assert_ptrdiff(0, ==, rv);

  rv = nghttp2_session_mem_recv2(session, bufs.head->buf.pos,
                                 nghttp2_buf_len(&bufs.head->buf));

  assert_ptrdiff((nghttp2_ssize)nghttp2_buf_len(&bufs.head->buf), ==, rv);

  item = nghttp2_session_get_next_ob_item(session);

  assert_null(item);

  nghttp2_bufs_reset(&bufs);
  nghttp2_hd_deflate_free(&deflater);
  nghttp2_session_del(session);
  nghttp2_option_del(option);

  nghttp2_bufs_free(&bufs);
}
