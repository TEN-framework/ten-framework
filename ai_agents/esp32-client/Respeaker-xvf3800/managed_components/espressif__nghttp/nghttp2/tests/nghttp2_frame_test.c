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
#include "nghttp2_frame_test.h"

#include <assert.h>
#include <stdio.h>

#include "munit.h"

#include "nghttp2_frame.h"
#include "nghttp2_helper.h"
#include "nghttp2_test_helper.h"
#include "nghttp2_priority_spec.h"

static MunitTest tests[] = {
  munit_void_test(test_nghttp2_frame_pack_headers),
  munit_void_test(test_nghttp2_frame_pack_headers_frame_too_large),
  munit_void_test(test_nghttp2_frame_pack_priority),
  munit_void_test(test_nghttp2_frame_pack_rst_stream),
  munit_void_test(test_nghttp2_frame_pack_settings),
  munit_void_test(test_nghttp2_frame_pack_push_promise),
  munit_void_test(test_nghttp2_frame_pack_ping),
  munit_void_test(test_nghttp2_frame_pack_goaway),
  munit_void_test(test_nghttp2_frame_pack_window_update),
  munit_void_test(test_nghttp2_frame_pack_altsvc),
  munit_void_test(test_nghttp2_frame_pack_origin),
  munit_void_test(test_nghttp2_frame_pack_priority_update),
  munit_void_test(test_nghttp2_nv_array_copy),
  munit_void_test(test_nghttp2_iv_check),
  munit_test_end(),
};

const MunitSuite frame_suite = {
  "/frame", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

static nghttp2_nv make_nv(const char *name, const char *value) {
  nghttp2_nv nv;
  nv.name = (uint8_t *)name;
  nv.value = (uint8_t *)value;
  nv.namelen = strlen(name);
  nv.valuelen = strlen(value);
  nv.flags = NGHTTP2_NV_FLAG_NONE;

  return nv;
}

#define HEADERS_LENGTH 7

static nghttp2_nv *headers(nghttp2_mem *mem) {
  nghttp2_nv *nva = mem->malloc(sizeof(nghttp2_nv) * HEADERS_LENGTH, NULL);
  nva[0] = make_nv("method", "GET");
  nva[1] = make_nv("scheme", "https");
  nva[2] = make_nv("url", "/");
  nva[3] = make_nv("x-head", "foo");
  nva[4] = make_nv("x-head", "bar");
  nva[5] = make_nv("version", "HTTP/1.1");
  nva[6] = make_nv("x-empty", "");
  return nva;
}

static void check_frame_header(size_t length, uint8_t type, uint8_t flags,
                               int32_t stream_id, nghttp2_frame_hd *hd) {
  assert_size(length, ==, hd->length);
  assert_uint8(type, ==, hd->type);
  assert_uint8(flags, ==, hd->flags);
  assert_int32(stream_id, ==, hd->stream_id);
  assert_uint8(0, ==, hd->reserved);
}

void test_nghttp2_frame_pack_headers(void) {
  nghttp2_hd_deflater deflater;
  nghttp2_hd_inflater inflater;
  nghttp2_headers frame, oframe;
  nghttp2_bufs bufs;
  nghttp2_nv *nva;
  nghttp2_priority_spec pri_spec;
  size_t nvlen;
  nva_out out;
  size_t hdblocklen;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nva_out_init(&out);
  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_hd_inflate_init(&inflater, mem);

  nva = headers(mem);
  nvlen = HEADERS_LENGTH;

  nghttp2_priority_spec_default_init(&pri_spec);

  nghttp2_frame_headers_init(
    &frame, NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS, 1000000007,
    NGHTTP2_HCAT_REQUEST, &pri_spec, nva, nvlen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame, &deflater);

  nghttp2_bufs_rewind(&bufs);

  assert_int(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));

  check_frame_header(
    nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN, NGHTTP2_HEADERS,
    NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS, 1000000007, &oframe.hd);
  /* We did not include PRIORITY flag */
  assert_int32(NGHTTP2_DEFAULT_WEIGHT, ==, oframe.pri_spec.weight);

  hdblocklen = nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN;
  assert_ptrdiff((nghttp2_ssize)hdblocklen, ==,
                 inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem));

  assert_size(7, ==, out.nvlen);
  assert_true(nvnameeq("method", &out.nva[0]));
  assert_true(nvvalueeq("GET", &out.nva[0]));

  nghttp2_frame_headers_free(&oframe, mem);
  nva_out_reset(&out, mem);
  nghttp2_bufs_reset(&bufs);

  memset(&oframe, 0, sizeof(oframe));
  /* Next, include NGHTTP2_FLAG_PRIORITY */
  nghttp2_priority_spec_init(&frame.pri_spec, 1000000009, 12, 1);
  frame.hd.flags |= NGHTTP2_FLAG_PRIORITY;

  rv = nghttp2_frame_pack_headers(&bufs, &frame, &deflater);

  assert_int(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));

  check_frame_header(
    nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN, NGHTTP2_HEADERS,
    NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PRIORITY,
    1000000007, &oframe.hd);

  assert_int32(1000000009, ==, oframe.pri_spec.stream_id);
  assert_int32(12, ==, oframe.pri_spec.weight);
  assert_true(oframe.pri_spec.exclusive);

  hdblocklen = nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN -
               nghttp2_frame_priority_len(oframe.hd.flags);
  assert_ptrdiff((nghttp2_ssize)hdblocklen, ==,
                 inflate_hd(&inflater, &out, &bufs,
                            NGHTTP2_FRAME_HDLEN +
                              nghttp2_frame_priority_len(oframe.hd.flags),
                            mem));

  nghttp2_nv_array_sort(out.nva, out.nvlen);
  assert_true(nvnameeq("method", &out.nva[0]));

  nghttp2_frame_headers_free(&oframe, mem);
  nva_out_reset(&out, mem);
  nghttp2_bufs_reset(&bufs);

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_hd_deflate_free(&deflater);
}

void test_nghttp2_frame_pack_headers_frame_too_large(void) {
  nghttp2_hd_deflater deflater;
  nghttp2_headers frame;
  nghttp2_bufs bufs;
  nghttp2_nv *nva;
  size_t big_vallen = NGHTTP2_HD_MAX_NV;
  nghttp2_nv big_hds[16];
  size_t big_hdslen = ARRLEN(big_hds);
  size_t i;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  for (i = 0; i < big_hdslen; ++i) {
    big_hds[i].name = (uint8_t *)"header";
    big_hds[i].value = mem->malloc(big_vallen + 1, NULL);
    memset(big_hds[i].value, '0' + (int)i, big_vallen);
    big_hds[i].value[big_vallen] = '\0';
    big_hds[i].namelen = strlen((char *)big_hds[i].name);
    big_hds[i].valuelen = big_vallen;
    big_hds[i].flags = NGHTTP2_NV_FLAG_NONE;
  }

  nghttp2_nv_array_copy(&nva, big_hds, big_hdslen, mem);
  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_frame_headers_init(
    &frame, NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS, 1000000007,
    NGHTTP2_HCAT_REQUEST, NULL, nva, big_hdslen);
  rv = nghttp2_frame_pack_headers(&bufs, &frame, &deflater);
  assert_int(NGHTTP2_ERR_HEADER_COMP, ==, rv);

  nghttp2_frame_headers_free(&frame, mem);
  nghttp2_bufs_free(&bufs);
  for (i = 0; i < big_hdslen; ++i) {
    mem->free(big_hds[i].value, NULL);
  }
  nghttp2_hd_deflate_free(&deflater);
}

void test_nghttp2_frame_pack_priority(void) {
  nghttp2_priority frame, oframe;
  nghttp2_bufs bufs;
  nghttp2_priority_spec pri_spec;

  frame_pack_bufs_init(&bufs);

  /* First, pack priority with priority group and weight */
  nghttp2_priority_spec_init(&pri_spec, 1000000009, 12, 1);

  nghttp2_frame_priority_init(&frame, 1000000007, &pri_spec);
  nghttp2_frame_pack_priority(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + 5, ==, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(5, NGHTTP2_PRIORITY, NGHTTP2_FLAG_NONE, 1000000007,
                     &oframe.hd);

  assert_int32(1000000009, ==, oframe.pri_spec.stream_id);
  assert_int32(12, ==, oframe.pri_spec.weight);
  assert_true(oframe.pri_spec.exclusive);

  nghttp2_frame_priority_free(&oframe);
  nghttp2_bufs_reset(&bufs);

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_priority_free(&frame);
}

void test_nghttp2_frame_pack_rst_stream(void) {
  nghttp2_rst_stream frame, oframe;
  nghttp2_bufs bufs;

  frame_pack_bufs_init(&bufs);

  nghttp2_frame_rst_stream_init(&frame, 1000000007, NGHTTP2_PROTOCOL_ERROR);
  nghttp2_frame_pack_rst_stream(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + 4, ==, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(4, NGHTTP2_RST_STREAM, NGHTTP2_FLAG_NONE, 1000000007,
                     &oframe.hd);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, oframe.error_code);

  nghttp2_frame_rst_stream_free(&oframe);
  nghttp2_bufs_reset(&bufs);

  /* Unknown error code is passed to callback as is */
  frame.error_code = 1000000009;
  nghttp2_frame_pack_rst_stream(&bufs, &frame);

  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));

  check_frame_header(4, NGHTTP2_RST_STREAM, NGHTTP2_FLAG_NONE, 1000000007,
                     &oframe.hd);

  assert_uint32(1000000009, ==, oframe.error_code);

  nghttp2_frame_rst_stream_free(&oframe);

  nghttp2_frame_rst_stream_free(&frame);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_frame_pack_settings(void) {
  nghttp2_settings frame, oframe;
  nghttp2_bufs bufs;
  int i;
  int rv;
  nghttp2_settings_entry iv[] = {{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 256},
                                 {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 16384},
                                 {NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, 4096}};
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nghttp2_frame_settings_init(&frame, NGHTTP2_FLAG_NONE,
                              nghttp2_frame_iv_copy(iv, 3, mem), 3);
  rv = nghttp2_frame_pack_settings(&bufs, &frame);

  assert_int(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN + 3 * NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH, ==,
              nghttp2_bufs_len(&bufs));

  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(3 * NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH, NGHTTP2_SETTINGS,
                     NGHTTP2_FLAG_NONE, 0, &oframe.hd);
  assert_size(3, ==, oframe.niv);
  for (i = 0; i < 3; ++i) {
    assert_int32(iv[i].settings_id, ==, oframe.iv[i].settings_id);
    assert_uint32(iv[i].value, ==, oframe.iv[i].value);
  }

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_settings_free(&frame, mem);
  nghttp2_frame_settings_free(&oframe, mem);
}

void test_nghttp2_frame_pack_push_promise(void) {
  nghttp2_hd_deflater deflater;
  nghttp2_hd_inflater inflater;
  nghttp2_push_promise frame, oframe;
  nghttp2_bufs bufs;
  nghttp2_nv *nva;
  size_t nvlen;
  nva_out out;
  size_t hdblocklen;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  nva_out_init(&out);
  nghttp2_hd_deflate_init(&deflater, mem);
  nghttp2_hd_inflate_init(&inflater, mem);

  nva = headers(mem);
  nvlen = HEADERS_LENGTH;
  nghttp2_frame_push_promise_init(&frame, NGHTTP2_FLAG_END_HEADERS, 1000000007,
                                  (1U << 31) - 1, nva, nvlen);
  rv = nghttp2_frame_pack_push_promise(&bufs, &frame, &deflater);

  assert_int(0, ==, rv);
  assert_size(0, <, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));

  check_frame_header(nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN,
                     NGHTTP2_PUSH_PROMISE, NGHTTP2_FLAG_END_HEADERS, 1000000007,
                     &oframe.hd);
  assert_int32((1U << 31) - 1, ==, oframe.promised_stream_id);

  hdblocklen = nghttp2_bufs_len(&bufs) - NGHTTP2_FRAME_HDLEN - 4;
  assert_ptrdiff(
    (nghttp2_ssize)hdblocklen, ==,
    inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN + 4, mem));

  assert_size(7, ==, out.nvlen);
  assert_true(nvnameeq("method", &out.nva[0]));
  assert_true(nvvalueeq("GET", &out.nva[0]));

  nva_out_reset(&out, mem);
  nghttp2_bufs_free(&bufs);
  nghttp2_frame_push_promise_free(&oframe, mem);
  nghttp2_frame_push_promise_free(&frame, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_hd_deflate_free(&deflater);
}

void test_nghttp2_frame_pack_ping(void) {
  nghttp2_ping frame, oframe;
  nghttp2_bufs bufs;
  const uint8_t opaque_data[] = "01234567";

  frame_pack_bufs_init(&bufs);

  nghttp2_frame_ping_init(&frame, NGHTTP2_FLAG_ACK, opaque_data);
  nghttp2_frame_pack_ping(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + 8, ==, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(8, NGHTTP2_PING, NGHTTP2_FLAG_ACK, 0, &oframe.hd);
  assert_memory_equal(sizeof(opaque_data) - 1, opaque_data, oframe.opaque_data);

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_ping_free(&oframe);
  nghttp2_frame_ping_free(&frame);
}

void test_nghttp2_frame_pack_goaway(void) {
  nghttp2_goaway frame, oframe;
  nghttp2_bufs bufs;
  size_t opaque_data_len = 16;
  uint8_t *opaque_data;
  int rv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();
  frame_pack_bufs_init(&bufs);

  opaque_data = mem->malloc(opaque_data_len, NULL);
  memcpy(opaque_data, "0123456789abcdef", opaque_data_len);
  nghttp2_frame_goaway_init(&frame, 1000000007, NGHTTP2_PROTOCOL_ERROR,
                            opaque_data, opaque_data_len);
  rv = nghttp2_frame_pack_goaway(&bufs, &frame);

  assert_int(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN + 8 + opaque_data_len, ==,
              nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(24, NGHTTP2_GOAWAY, NGHTTP2_FLAG_NONE, 0, &oframe.hd);
  assert_int32(1000000007, ==, oframe.last_stream_id);
  assert_uint32(NGHTTP2_PROTOCOL_ERROR, ==, oframe.error_code);

  assert_size(opaque_data_len, ==, oframe.opaque_data_len);
  assert_memory_equal(opaque_data_len, opaque_data, oframe.opaque_data);

  nghttp2_frame_goaway_free(&oframe, mem);
  nghttp2_bufs_reset(&bufs);

  /* Unknown error code is passed to callback as is */
  frame.error_code = 1000000009;

  rv = nghttp2_frame_pack_goaway(&bufs, &frame);

  assert_int(0, ==, rv);
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(24, NGHTTP2_GOAWAY, NGHTTP2_FLAG_NONE, 0, &oframe.hd);
  assert_uint32(1000000009, ==, oframe.error_code);

  nghttp2_frame_goaway_free(&oframe, mem);

  nghttp2_frame_goaway_free(&frame, mem);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_frame_pack_window_update(void) {
  nghttp2_window_update frame, oframe;
  nghttp2_bufs bufs;

  frame_pack_bufs_init(&bufs);

  nghttp2_frame_window_update_init(&frame, NGHTTP2_FLAG_NONE, 1000000007, 4096);
  nghttp2_frame_pack_window_update(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + 4, ==, nghttp2_bufs_len(&bufs));
  assert_int(0, ==, unpack_framebuf((nghttp2_frame *)&oframe, &bufs));
  check_frame_header(4, NGHTTP2_WINDOW_UPDATE, NGHTTP2_FLAG_NONE, 1000000007,
                     &oframe.hd);
  assert_int32(4096, ==, oframe.window_size_increment);

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_window_update_free(&oframe);
  nghttp2_frame_window_update_free(&frame);
}

void test_nghttp2_frame_pack_altsvc(void) {
  nghttp2_extension frame, oframe;
  nghttp2_ext_altsvc altsvc, oaltsvc;
  nghttp2_bufs bufs;
  int rv;
  size_t payloadlen;
  static const uint8_t origin[] = "nghttp2.org";
  static const uint8_t field_value[] = "h2=\":443\"";
  nghttp2_buf buf;
  uint8_t *rawbuf;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  frame_pack_bufs_init(&bufs);

  frame.payload = &altsvc;
  oframe.payload = &oaltsvc;

  rawbuf = nghttp2_mem_malloc(mem, 32);
  nghttp2_buf_wrap_init(&buf, rawbuf, 32);

  buf.last = nghttp2_cpymem(buf.last, origin, sizeof(origin) - 1);
  buf.last = nghttp2_cpymem(buf.last, field_value, sizeof(field_value) - 1);

  nghttp2_frame_altsvc_init(&frame, 1000000007, buf.pos, sizeof(origin) - 1,
                            buf.pos + sizeof(origin) - 1,
                            sizeof(field_value) - 1);

  payloadlen = 2 + sizeof(origin) - 1 + sizeof(field_value) - 1;

  nghttp2_frame_pack_altsvc(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + payloadlen, ==, nghttp2_bufs_len(&bufs));

  rv = unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

  assert_int(0, ==, rv);

  check_frame_header(payloadlen, NGHTTP2_ALTSVC, NGHTTP2_FLAG_NONE, 1000000007,
                     &oframe.hd);

  assert_size(sizeof(origin) - 1, ==, oaltsvc.origin_len);
  assert_memory_equal(sizeof(origin) - 1, origin, oaltsvc.origin);
  assert_size(sizeof(field_value) - 1, ==, oaltsvc.field_value_len);
  assert_memory_equal(sizeof(field_value) - 1, field_value,
                      oaltsvc.field_value);

  nghttp2_frame_altsvc_free(&oframe, mem);
  nghttp2_frame_altsvc_free(&frame, mem);
  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_frame_pack_origin(void) {
  nghttp2_extension frame, oframe;
  nghttp2_ext_origin origin, oorigin;
  nghttp2_bufs bufs;
  nghttp2_buf *buf;
  int rv;
  size_t payloadlen;
  static const uint8_t example[] = "https://example.com";
  static const uint8_t nghttp2[] = "https://nghttp2.org";
  nghttp2_origin_entry ov[] = {
    {
      (uint8_t *)example,
      sizeof(example) - 1,
    },
    {
      NULL,
      0,
    },
    {
      (uint8_t *)nghttp2,
      sizeof(nghttp2) - 1,
    },
  };
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  frame_pack_bufs_init(&bufs);

  frame.payload = &origin;
  oframe.payload = &oorigin;

  nghttp2_frame_origin_init(&frame, ov, 3);

  payloadlen = 2 + sizeof(example) - 1 + 2 + 2 + sizeof(nghttp2) - 1;

  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_int(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN + payloadlen, ==, nghttp2_bufs_len(&bufs));

  rv = unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

  assert_int(0, ==, rv);

  check_frame_header(payloadlen, NGHTTP2_ORIGIN, NGHTTP2_FLAG_NONE, 0,
                     &oframe.hd);

  assert_size(2, ==, oorigin.nov);
  assert_size(sizeof(example) - 1, ==, oorigin.ov[0].origin_len);
  assert_memory_equal(sizeof(example) - 1, example, oorigin.ov[0].origin);
  assert_size(sizeof(nghttp2) - 1, ==, oorigin.ov[1].origin_len);
  assert_memory_equal(sizeof(nghttp2) - 1, nghttp2, oorigin.ov[1].origin);

  nghttp2_frame_origin_free(&oframe, mem);

  /* Check the case where origin length is too large */
  buf = &bufs.head->buf;
  nghttp2_put_uint16be(buf->pos + NGHTTP2_FRAME_HDLEN,
                       (uint16_t)(payloadlen - 1));

  rv = unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

  assert_int(NGHTTP2_ERR_FRAME_SIZE_ERROR, ==, rv);

  nghttp2_bufs_reset(&bufs);
  memset(&oframe, 0, sizeof(oframe));
  memset(&oorigin, 0, sizeof(oorigin));
  oframe.payload = &oorigin;

  /* Empty ORIGIN frame */
  nghttp2_frame_origin_init(&frame, NULL, 0);

  rv = nghttp2_frame_pack_origin(&bufs, &frame);

  assert_int(0, ==, rv);
  assert_size(NGHTTP2_FRAME_HDLEN, ==, nghttp2_bufs_len(&bufs));

  rv = unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

  assert_int(0, ==, rv);

  check_frame_header(0, NGHTTP2_ORIGIN, NGHTTP2_FLAG_NONE, 0, &oframe.hd);

  assert_size(0, ==, oorigin.nov);
  assert_null(oorigin.ov);

  nghttp2_frame_origin_free(&oframe, mem);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_frame_pack_priority_update(void) {
  nghttp2_extension frame, oframe;
  nghttp2_ext_priority_update priority_update, opriority_update;
  nghttp2_bufs bufs;
  int rv;
  size_t payloadlen;
  static const uint8_t field_value[] = "i,u=0";

  frame_pack_bufs_init(&bufs);

  frame.payload = &priority_update;
  oframe.payload = &opriority_update;

  nghttp2_frame_priority_update_init(&frame, 1000000007, (uint8_t *)field_value,
                                     sizeof(field_value) - 1);

  payloadlen = 4 + sizeof(field_value) - 1;

  nghttp2_frame_pack_priority_update(&bufs, &frame);

  assert_size(NGHTTP2_FRAME_HDLEN + payloadlen, ==, nghttp2_bufs_len(&bufs));

  rv = unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

  assert_int(0, ==, rv);

  check_frame_header(payloadlen, NGHTTP2_PRIORITY_UPDATE, NGHTTP2_FLAG_NONE, 0,
                     &oframe.hd);

  assert_size(sizeof(field_value) - 1, ==, opriority_update.field_value_len);
  assert_memory_equal(sizeof(field_value) - 1, field_value,
                      opriority_update.field_value);

  nghttp2_bufs_free(&bufs);
}

void test_nghttp2_nv_array_copy(void) {
  nghttp2_nv *nva;
  int rv;
  nghttp2_nv emptynv[] = {MAKE_NV("", ""), MAKE_NV("", "")};
  nghttp2_nv nv[] = {MAKE_NV("alpha", "bravo"), MAKE_NV("charlie", "delta")};
  nghttp2_nv bignv;
  nghttp2_mem *mem;

  mem = nghttp2_mem_default();

  bignv.name = (uint8_t *)"echo";
  bignv.namelen = strlen("echo");
  bignv.valuelen = (1 << 14) - 1;
  bignv.value = mem->malloc(bignv.valuelen, NULL);
  bignv.flags = NGHTTP2_NV_FLAG_NONE;
  memset(bignv.value, '0', bignv.valuelen);

  rv = nghttp2_nv_array_copy(&nva, NULL, 0, mem);
  assert_int(0, ==, rv);
  assert_null(nva);

  rv = nghttp2_nv_array_copy(&nva, emptynv, ARRLEN(emptynv), mem);
  assert_int(0, ==, rv);
  assert_size(0, ==, nva[0].namelen);
  assert_size(0, ==, nva[0].valuelen);
  assert_size(0, ==, nva[1].namelen);
  assert_size(0, ==, nva[1].valuelen);

  nghttp2_nv_array_del(nva, mem);

  rv = nghttp2_nv_array_copy(&nva, nv, ARRLEN(nv), mem);
  assert_int(0, ==, rv);
  assert_size(5, ==, nva[0].namelen);
  assert_memory_equal(5, "alpha", nva[0].name);
  assert_size(5, ==, nva[0].valuelen);
  assert_memory_equal(5, "bravo", nva[0].value);
  assert_size(7, ==, nva[1].namelen);
  assert_memory_equal(7, "charlie", nva[1].name);
  assert_size(5, ==, nva[1].valuelen);
  assert_memory_equal(5, "delta", nva[1].value);

  nghttp2_nv_array_del(nva, mem);

  /* Large header field is acceptable */
  rv = nghttp2_nv_array_copy(&nva, &bignv, 1, mem);
  assert_int(0, ==, rv);

  nghttp2_nv_array_del(nva, mem);

  mem->free(bignv.value, NULL);
}

void test_nghttp2_iv_check(void) {
  nghttp2_settings_entry iv[5];

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 100;
  iv[1].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[1].value = 1024;

  assert_true(nghttp2_iv_check(iv, 2));

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = NGHTTP2_MAX_WINDOW_SIZE;
  assert_true(nghttp2_iv_check(iv, 2));

  /* Too large window size */
  iv[1].value = (uint32_t)NGHTTP2_MAX_WINDOW_SIZE + 1;
  assert_false(nghttp2_iv_check(iv, 2));

  /* ENABLE_PUSH only allows 0 or 1 */
  iv[1].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
  iv[1].value = 0;
  assert_true(nghttp2_iv_check(iv, 2));
  iv[1].value = 1;
  assert_true(nghttp2_iv_check(iv, 2));
  iv[1].value = 3;
  assert_false(nghttp2_iv_check(iv, 2));

  /* Undefined SETTINGS ID is allowed */
  iv[1].settings_id = 1000000009;
  iv[1].value = 0;
  assert_true(nghttp2_iv_check(iv, 2));

  /* Full size SETTINGS_HEADER_TABLE_SIZE (UINT32_MAX) must be
     accepted */
  iv[1].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
  iv[1].value = UINT32_MAX;
  assert_true(nghttp2_iv_check(iv, 2));

  /* Too small SETTINGS_MAX_FRAME_SIZE */
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
  iv[0].value = NGHTTP2_MAX_FRAME_SIZE_MIN - 1;
  assert_false(nghttp2_iv_check(iv, 1));

  /* Too large SETTINGS_MAX_FRAME_SIZE */
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
  iv[0].value = NGHTTP2_MAX_FRAME_SIZE_MAX + 1;
  assert_false(nghttp2_iv_check(iv, 1));

  /* Max and min SETTINGS_MAX_FRAME_SIZE */
  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
  iv[0].value = NGHTTP2_MAX_FRAME_SIZE_MIN;
  iv[1].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
  iv[1].value = NGHTTP2_MAX_FRAME_SIZE_MAX;
  assert_true(nghttp2_iv_check(iv, 2));
}
