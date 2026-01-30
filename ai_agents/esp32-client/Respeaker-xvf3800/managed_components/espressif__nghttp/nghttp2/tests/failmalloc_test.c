/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012, 2014 Tatsuhiro Tsujikawa
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
#include "failmalloc_test.h"

#include <stdio.h>
#include <assert.h>

#include "munit.h"

#include "nghttp2_session.h"
#include "nghttp2_stream.h"
#include "nghttp2_frame.h"
#include "nghttp2_helper.h"
#include "malloc_wrapper.h"
#include "nghttp2_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_session_send),
  munit_void_test(test_nghttp2_session_send_server),
  munit_void_test(test_nghttp2_session_recv),
  munit_void_test(test_nghttp2_frame),
  munit_void_test(test_nghttp2_hd),
  munit_test_end(),
};

const MunitSuite failmalloc_suite = {
  "/failmalloc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

typedef struct {
  uint8_t data[8192];
  uint8_t *datamark, *datalimit;
} data_feed;

typedef struct {
  data_feed *df;
  size_t data_source_length;
} my_user_data;

static void data_feed_init(data_feed *df, nghttp2_bufs *bufs) {
  nghttp2_buf *buf;
  size_t data_length;

  buf = &bufs->head->buf;
  data_length = nghttp2_buf_len(buf);

  assert(data_length <= sizeof(df->data));
  memcpy(df->data, buf->pos, data_length);
  df->datamark = df->data;
  df->datalimit = df->data + data_length;
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

static nghttp2_ssize data_feed_recv_callback(nghttp2_session *session,
                                             uint8_t *data, size_t len,
                                             int flags, void *user_data) {
  data_feed *df = ((my_user_data *)user_data)->df;
  size_t avail = (size_t)(df->datalimit - df->datamark);
  size_t wlen = nghttp2_min_size(avail, len);
  (void)session;
  (void)flags;

  memcpy(data, df->datamark, wlen);
  df->datamark += wlen;
  return (nghttp2_ssize)wlen;
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
    *data_flags = NGHTTP2_DATA_FLAG_EOF;
  }
  return (nghttp2_ssize)wlen;
}

#define TEST_FAILMALLOC_RUN(FUN)                                               \
  do {                                                                         \
    int nmalloc, i;                                                            \
                                                                               \
    nghttp2_failmalloc = 0;                                                    \
    nghttp2_nmalloc = 0;                                                       \
    FUN();                                                                     \
    nmalloc = nghttp2_nmalloc;                                                 \
                                                                               \
    nghttp2_failmalloc = 1;                                                    \
    for (i = 0; i < nmalloc; ++i) {                                            \
      nghttp2_nmalloc = 0;                                                     \
      nghttp2_failstart = i;                                                   \
      /* printf("i=%zu\n", i); */                                              \
      FUN();                                                                   \
      /* printf("nmalloc=%d\n", nghttp2_nmalloc); */                           \
    }                                                                          \
    nghttp2_failmalloc = 0;                                                    \
  } while (0)

static void run_nghttp2_session_send(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_nv nv[] = {MAKE_NV(":host", "example.org"),
                     MAKE_NV(":scheme", "https")};
  nghttp2_data_provider2 data_prd;
  nghttp2_settings_entry iv[2];
  my_user_data ud;
  int rv;
  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.send_callback2 = null_send_callback;

  data_prd.read_callback = fixed_length_data_source_read_callback;
  ud.data_source_length = 64 * 1024;

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 4096;
  iv[1].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[1].value = 100;

  rv = nghttp2_session_client_new3(&session, &callbacks, &ud, NULL,
                                   nghttp2_mem_fm());
  if (rv != 0) {
    goto client_new_fail;
  }
  rv = nghttp2_submit_request2(session, NULL, nv, ARRLEN(nv), &data_prd, NULL);
  if (rv < 0) {
    goto fail;
  }
  rv = nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, -1, NULL, nv,
                              ARRLEN(nv), NULL);
  if (rv < 0) {
    goto fail;
  }
  rv = nghttp2_session_send(session);
  if (rv != 0) {
    goto fail;
  }
  /* The HEADERS submitted by the previous nghttp2_submit_headers will
     have stream ID 3. Send HEADERS to that stream. */
  rv = nghttp2_submit_headers(session, NGHTTP2_FLAG_NONE, 3, NULL, nv,
                              ARRLEN(nv), NULL);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_submit_data2(session, NGHTTP2_FLAG_END_STREAM, 3, &data_prd);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_session_send(session);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, 3, NGHTTP2_CANCEL);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_session_send(session);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, NULL);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, 2);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_session_send(session);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_submit_goaway(session, NGHTTP2_FLAG_NONE, 100, NGHTTP2_NO_ERROR,
                             NULL, 0);
  if (rv != 0) {
    goto fail;
  }
  rv = nghttp2_session_send(session);
  if (rv != 0) {
    goto fail;
  }

fail:
  nghttp2_session_del(session);
client_new_fail:;
}

void test_nghttp2_session_send(void) {
  TEST_FAILMALLOC_RUN(run_nghttp2_session_send);
}

static void run_nghttp2_session_send_server(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks *callbacks;
  int rv;
  const uint8_t *txdata;
  nghttp2_ssize txdatalen;
  const uint8_t origin[] = "nghttp2.org";
  const uint8_t altsvc_field_value[] = "h2=\":443\"";
  static const uint8_t nghttp2[] = "https://nghttp2.org";
  static const nghttp2_origin_entry ov = {
    (uint8_t *)nghttp2,
    sizeof(nghttp2) - 1,
  };

  rv = nghttp2_session_callbacks_new(&callbacks);
  if (rv != 0) {
    return;
  }

  rv = nghttp2_session_server_new3(&session, callbacks, NULL, NULL,
                                   nghttp2_mem_fm());

  nghttp2_session_callbacks_del(callbacks);

  if (rv != 0) {
    return;
  }

  rv = nghttp2_submit_altsvc(session, NGHTTP2_FLAG_NONE, 0, origin,
                             sizeof(origin) - 1, altsvc_field_value,
                             sizeof(altsvc_field_value) - 1);
  if (rv != 0) {
    goto fail;
  }

  rv = nghttp2_submit_origin(session, NGHTTP2_FLAG_NONE, &ov, 1);
  if (rv != 0) {
    goto fail;
  }

  txdatalen = nghttp2_session_mem_send2(session, &txdata);

  if (txdatalen < 0) {
    goto fail;
  }

fail:
  nghttp2_session_del(session);
}

void test_nghttp2_session_send_server(void) {
  TEST_FAILMALLOC_RUN(run_nghttp2_session_send_server);
}

static void run_nghttp2_session_recv(void) {
  nghttp2_session *session;
  nghttp2_session_callbacks callbacks;
  nghttp2_hd_deflater deflater;
  nghttp2_frame frame;
  nghttp2_bufs bufs;
  nghttp2_nv nv[] = {
    MAKE_NV(":method", "GET"),
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":authority", "example.org"),
    MAKE_NV(":path", "/"),
  };
  nghttp2_settings_entry iv[2];
  my_user_data ud;
  data_feed df;
  int rv;
  nghttp2_nv *nva;
  size_t nvlen;

  rv = frame_pack_bufs_init(&bufs);

  if (rv != 0) {
    return;
  }

  memset(&callbacks, 0, sizeof(nghttp2_session_callbacks));
  callbacks.recv_callback2 = data_feed_recv_callback;
  ud.df = &df;

  nghttp2_failmalloc_pause();
  nghttp2_hd_deflate_init(&deflater, nghttp2_mem_fm());
  nghttp2_session_server_new3(&session, &callbacks, &ud, NULL,
                              nghttp2_mem_fm());

  /* Client preface */
  nghttp2_bufs_add(&bufs, NGHTTP2_CLIENT_MAGIC, NGHTTP2_CLIENT_MAGIC_LEN);
  data_feed_init(&df, &bufs);
  nghttp2_bufs_reset(&bufs);
  nghttp2_failmalloc_unpause();

  rv = nghttp2_session_recv(session);
  if (rv != 0) {
    goto fail;
  }

  nghttp2_failmalloc_pause();
  /* SETTINGS */
  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 4096;
  iv[1].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[1].value = 100;
  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE,
                              nghttp2_frame_iv_copy(iv, 2, nghttp2_mem_fm()),
                              2);
  nghttp2_frame_pack_settings(&bufs, &frame.settings);
  nghttp2_frame_settings_free(&frame.settings, nghttp2_mem_fm());
  data_feed_init(&df, &bufs);
  nghttp2_bufs_reset(&bufs);
  nghttp2_failmalloc_unpause();

  rv = nghttp2_session_recv(session);
  if (rv != 0) {
    goto fail;
  }

  nghttp2_failmalloc_pause();
  /* HEADERS */
  nvlen = ARRLEN(nv);
  nghttp2_nv_array_copy(&nva, nv, nvlen, nghttp2_mem_fm());
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_STREAM, 1,
                             NGHTTP2_HCAT_REQUEST, NULL, nva, nvlen);
  nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);
  nghttp2_frame_headers_free(&frame.headers, nghttp2_mem_fm());
  data_feed_init(&df, &bufs);
  nghttp2_bufs_reset(&bufs);
  nghttp2_failmalloc_unpause();

  rv = nghttp2_session_recv(session);
  if (rv != 0) {
    goto fail;
  }

  /* PING */
  nghttp2_failmalloc_pause();
  nghttp2_frame_ping_init(&frame.ping, NGHTTP2_FLAG_NONE, NULL);
  nghttp2_frame_pack_ping(&bufs, &frame.ping);
  nghttp2_frame_ping_free(&frame.ping);
  data_feed_init(&df, &bufs);
  nghttp2_bufs_reset(&bufs);

  nghttp2_failmalloc_unpause();

  rv = nghttp2_session_recv(session);
  if (rv != 0) {
    goto fail;
  }

  /* RST_STREAM */
  nghttp2_failmalloc_pause();
  nghttp2_frame_rst_stream_init(&frame.rst_stream, 1, NGHTTP2_PROTOCOL_ERROR);
  nghttp2_frame_pack_rst_stream(&bufs, &frame.rst_stream);
  nghttp2_frame_rst_stream_free(&frame.rst_stream);
  nghttp2_bufs_reset(&bufs);

  nghttp2_failmalloc_unpause();

  rv = nghttp2_session_recv(session);
  if (rv != 0) {
    goto fail;
  }

fail:
  nghttp2_bufs_free(&bufs);
  nghttp2_session_del(session);
  nghttp2_hd_deflate_free(&deflater);
}

void test_nghttp2_session_recv(void) {
  TEST_FAILMALLOC_RUN(run_nghttp2_session_recv);
}

static void run_nghttp2_frame_pack_headers(void) {
  nghttp2_hd_deflater deflater;
  nghttp2_hd_inflater inflater;
  nghttp2_frame frame, oframe;
  nghttp2_bufs bufs;
  nghttp2_nv nv[] = {MAKE_NV(":host", "example.org"),
                     MAKE_NV(":scheme", "https")};
  int rv;
  nghttp2_nv *nva;
  size_t nvlen;

  rv = frame_pack_bufs_init(&bufs);

  if (rv != 0) {
    return;
  }

  rv = nghttp2_hd_deflate_init(&deflater, nghttp2_mem_fm());
  if (rv != 0) {
    goto deflate_init_fail;
  }
  rv = nghttp2_hd_inflate_init(&inflater, nghttp2_mem_fm());
  if (rv != 0) {
    goto inflate_init_fail;
  }
  nvlen = ARRLEN(nv);
  rv = nghttp2_nv_array_copy(&nva, nv, nvlen, nghttp2_mem_fm());
  if (rv < 0) {
    goto nv_copy_fail;
  }
  nghttp2_frame_headers_init(&frame.headers, NGHTTP2_FLAG_END_STREAM, 1,
                             NGHTTP2_HCAT_REQUEST, NULL, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame.headers, &deflater);
  if (rv != 0) {
    goto fail;
  }
  rv = unpack_framebuf(&oframe, &bufs);
  if (rv != 0) {
    goto fail;
  }
  nghttp2_frame_headers_free(&oframe.headers, nghttp2_mem_fm());

fail:
  nghttp2_frame_headers_free(&frame.headers, nghttp2_mem_fm());
nv_copy_fail:
  nghttp2_hd_inflate_free(&inflater);
inflate_init_fail:
  nghttp2_hd_deflate_free(&deflater);
deflate_init_fail:
  nghttp2_bufs_free(&bufs);
}

static void run_nghttp2_frame_pack_settings(void) {
  nghttp2_frame frame, oframe;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  nghttp2_settings_entry iv[2], *iv_copy;
  int rv;

  rv = frame_pack_bufs_init(&bufs);

  if (rv != 0) {
    return;
  }

  iv[0].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[0].value = 4096;
  iv[1].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[1].value = 100;

  iv_copy = nghttp2_frame_iv_copy(iv, 2, nghttp2_mem_fm());

  if (iv_copy == NULL) {
    goto iv_copy_fail;
  }

  nghttp2_frame_settings_init(&frame.settings, NGHTTP2_FLAG_NONE, iv_copy, 2);

  rv = nghttp2_frame_pack_settings(&bufs, &frame.settings);

  if (rv != 0) {
    goto fail;
  }

  buf = &bufs.head->buf;

  rv = nghttp2_frame_unpack_settings_payload2(
    &oframe.settings.iv, &oframe.settings.niv, buf->pos + NGHTTP2_FRAME_HDLEN,
    nghttp2_buf_len(buf) - NGHTTP2_FRAME_HDLEN, nghttp2_mem_fm());

  if (rv != 0) {
    goto fail;
  }
  nghttp2_frame_settings_free(&oframe.settings, nghttp2_mem_fm());

fail:
  nghttp2_frame_settings_free(&frame.settings, nghttp2_mem_fm());
iv_copy_fail:
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_frame(void) {
  TEST_FAILMALLOC_RUN(run_nghttp2_frame_pack_headers);
  TEST_FAILMALLOC_RUN(run_nghttp2_frame_pack_settings);
}

static int deflate_inflate(nghttp2_hd_deflater *deflater,
                           nghttp2_hd_inflater *inflater, nghttp2_bufs *bufs,
                           nghttp2_nv *nva, size_t nvlen, nghttp2_mem *mem) {
  int rv;

  rv = nghttp2_hd_deflate_hd_bufs(deflater, bufs, nva, nvlen);

  if (rv != 0) {
    return rv;
  }

  rv = (int)inflate_hd(inflater, NULL, bufs, 0, mem);

  if (rv < 0) {
    return rv;
  }

  nghttp2_bufs_reset(bufs);

  return 0;
}

static void run_nghttp2_hd(void) {
  nghttp2_hd_deflater deflater;
  nghttp2_hd_inflater inflater;
  nghttp2_bufs bufs;
  int rv;
  nghttp2_nv nva1[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "example.org"),
    MAKE_NV(":path", "/slashdot"), MAKE_NV("accept-encoding", "gzip, deflate"),
    MAKE_NV("foo", "bar")};
  nghttp2_nv nva2[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "example.org"),
    MAKE_NV(":path", "/style.css"), MAKE_NV("cookie", "nghttp2=FTW"),
    MAKE_NV("foo", "bar2")};

  rv = frame_pack_bufs_init(&bufs);

  if (rv != 0) {
    return;
  }

  rv = nghttp2_hd_deflate_init(&deflater, nghttp2_mem_fm());

  if (rv != 0) {
    goto deflate_init_fail;
  }

  rv = nghttp2_hd_inflate_init(&inflater, nghttp2_mem_fm());

  if (rv != 0) {
    goto inflate_init_fail;
  }

  rv = deflate_inflate(&deflater, &inflater, &bufs, nva1, ARRLEN(nva1),
                       nghttp2_mem_fm());

  if (rv != 0) {
    goto deflate_hd_fail;
  }

  rv = deflate_inflate(&deflater, &inflater, &bufs, nva2, ARRLEN(nva2),
                       nghttp2_mem_fm());

  if (rv != 0) {
    goto deflate_hd_fail;
  }

deflate_hd_fail:
  nghttp2_hd_inflate_free(&inflater);
inflate_init_fail:
  nghttp2_hd_deflate_free(&deflater);
deflate_init_fail:
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_hd(void) { TEST_FAILMALLOC_RUN(run_nghttp2_hd); }
