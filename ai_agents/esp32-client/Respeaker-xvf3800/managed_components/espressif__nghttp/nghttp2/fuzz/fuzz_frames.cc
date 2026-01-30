#include <string>
#include <fuzzer/FuzzedDataProvider.h>

extern "C" {
#include <string.h>
#include "nghttp2_hd.h"
#include "nghttp2_frame.h"

#include "nghttp2_test_helper.h"

#define HEADERS_LENGTH 7

static nghttp2_nv fuzz_make_nv(std::string s1, std::string s2) {
  nghttp2_nv nv;
  uint8_t *n = (uint8_t *)malloc(s1.size());
  memcpy(n, s1.c_str(), s1.size());

  uint8_t *v = (uint8_t *)malloc(s2.size());
  memcpy(v, s2.c_str(), s2.size());

  nv.name = n;
  nv.value = v;
  nv.namelen = s1.size();
  nv.valuelen = s2.size();
  nv.flags = NGHTTP2_NV_FLAG_NONE;

  return nv;
}

static void fuzz_free_nv(nghttp2_nv *nv) {
  free(nv->name);
  free(nv->value);
}

void check_frame_pack_headers(FuzzedDataProvider *data_provider) {
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

  /* Create a set of headers seeded with data from the fuzzer */
  nva = (nghttp2_nv *)mem->malloc(sizeof(nghttp2_nv) * HEADERS_LENGTH, NULL);
  for (int i = 0; i < HEADERS_LENGTH; i++) {
    nva[i] = fuzz_make_nv(data_provider->ConsumeRandomLengthString(30),
                          data_provider->ConsumeRandomLengthString(300));
  }

  nvlen = HEADERS_LENGTH;
  nghttp2_priority_spec_default_init(&pri_spec);
  nghttp2_frame_headers_init(
    &frame, NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS, 1000000007,
    NGHTTP2_HCAT_REQUEST, &pri_spec, nva, nvlen);

  /* Perform a set of operations with the fuzz data */
  rv = nghttp2_frame_pack_headers(&bufs, &frame, &deflater);
  if (rv == 0) {
    unpack_framebuf((nghttp2_frame *)&oframe, &bufs);

    inflate_hd(&inflater, &out, &bufs, NGHTTP2_FRAME_HDLEN, mem);
    nva_out_reset(&out, mem);
    nghttp2_bufs_reset(&bufs);
  }

  nghttp2_nv *nva2 = NULL;
  rv = nghttp2_nv_array_copy(&nva2, nva, nvlen, mem);
  if (rv == 0) {
    nghttp2_nv_array_del(nva2, mem);
  }

  /* Cleanup */
  for (int i = 0; i < HEADERS_LENGTH; i++) {
    fuzz_free_nv(&nva[i]);
  }

  nghttp2_bufs_free(&bufs);
  nghttp2_frame_headers_free(&frame, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_hd_deflate_free(&deflater);
}

void check_frame_push_promise(FuzzedDataProvider *data_provider) {
  nghttp2_hd_deflater deflater;
  nghttp2_hd_inflater inflater;
  nghttp2_push_promise frame, oframe;
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

  /* Create a set of headers seeded with data from the fuzzer */
  nva = (nghttp2_nv *)mem->malloc(sizeof(nghttp2_nv) * HEADERS_LENGTH, NULL);
  for (int i = 0; i < HEADERS_LENGTH; i++) {
    nva[i] = fuzz_make_nv(data_provider->ConsumeRandomLengthString(30),
                          data_provider->ConsumeRandomLengthString(300));
  }
  nvlen = HEADERS_LENGTH;
  nghttp2_priority_spec_default_init(&pri_spec);

  /* Perform a set of operations with the fuzz data */
  nghttp2_frame_push_promise_init(&frame, NGHTTP2_FLAG_END_HEADERS, 1000000007,
                                  (1U << 31) - 1, nva, nvlen);

  rv = nghttp2_frame_pack_push_promise(&bufs, &frame, &deflater);
  if (rv == 0) {
    unpack_framebuf((nghttp2_frame *)&oframe, &bufs);
  }

  nghttp2_nv *nva2 = NULL;
  rv = nghttp2_nv_array_copy(&nva2, nva, nvlen, mem);
  if (rv == 0) {
    nghttp2_nv_array_del(nva2, mem);
  }

  /* Cleanup */
  for (int i = 0; i < HEADERS_LENGTH; i++) {
    fuzz_free_nv(&nva[i]);
  }

  nghttp2_bufs_reset(&bufs);
  nghttp2_bufs_free(&bufs);

  nghttp2_frame_push_promise_free(&frame, mem);
  nghttp2_hd_inflate_free(&inflater);
  nghttp2_hd_deflate_free(&deflater);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  check_frame_pack_headers(&data_provider);
  check_frame_push_promise(&data_provider);
  return 0;
}

} // extern C
