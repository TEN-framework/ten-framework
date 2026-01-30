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
#include "shrpx_downstream_test.h"

#include <iostream>

#include "munitxx.h"

#include "shrpx_downstream.h"

using namespace std::literals;

namespace shrpx {

namespace {
const MunitTest tests[]{
  munit_void_test(test_downstream_field_store_append_last_header),
  munit_void_test(test_downstream_field_store_header),
  munit_void_test(test_downstream_crumble_request_cookie),
  munit_void_test(test_downstream_assemble_request_cookie),
  munit_void_test(test_downstream_rewrite_location_response_header),
  munit_void_test(test_downstream_supports_non_final_response),
  munit_void_test(test_downstream_find_affinity_cookie),
  munit_test_end(),
};
} // namespace

const MunitSuite downstream_suite{
  "/downstream", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_downstream_field_store_append_last_header(void) {
  BlockAllocator balloc(16, 16);
  FieldStore fs(balloc, 0);
  fs.alloc_add_header_name("alpha"_sr);
  auto bravo = "BRAVO"_sr;
  fs.append_last_header_key(bravo.data(), bravo.size());
  // Add more characters so that relloc occurs
  auto golf = "golF0123456789"_sr;
  fs.append_last_header_key(golf.data(), golf.size());

  auto charlie = "Charlie"_sr;
  fs.append_last_header_value(charlie.data(), charlie.size());
  auto delta = "deltA"_sr;
  fs.append_last_header_value(delta.data(), delta.size());
  // Add more characters so that relloc occurs
  auto echo = "echo0123456789"_sr;
  fs.append_last_header_value(echo.data(), echo.size());

  fs.add_header_token("echo"_sr, "foxtrot"_sr, false, -1);

  auto ans =
    HeaderRefs{{"alphabravogolf0123456789"_sr, "CharliedeltAecho0123456789"_sr},
               {"echo"_sr, "foxtrot"_sr}};
  assert_true(ans == fs.headers());
}

void test_downstream_field_store_header(void) {
  BlockAllocator balloc(16, 16);
  FieldStore fs(balloc, 0);
  fs.add_header_token("alpha"_sr, "0"_sr, false, -1);
  fs.add_header_token(":authority"_sr, "1"_sr, false, http2::HD__AUTHORITY);
  fs.add_header_token("content-length"_sr, "2"_sr, false,
                      http2::HD_CONTENT_LENGTH);

  // By token
  assert_true(HeaderRef(StringRef{":authority"}, StringRef{"1"}) ==
              *fs.header(http2::HD__AUTHORITY));
  assert_null(fs.header(http2::HD__METHOD));

  // By name
  assert_true(HeaderRef(StringRef{"alpha"}, StringRef{"0"}) ==
              *fs.header("alpha"_sr));
  assert_null(fs.header("bravo"_sr));
}

void test_downstream_crumble_request_cookie(void) {
  Downstream d(nullptr, nullptr, 0);
  auto &req = d.request();
  req.fs.add_header_token(":method"_sr, "get"_sr, false, -1);
  req.fs.add_header_token(":path"_sr, "/"_sr, false, -1);
  req.fs.add_header_token("cookie"_sr, "alpha; bravo; ; ;; charlie;;"_sr, true,
                          http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, ";delta"_sr, false, http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "echo"_sr, false, http2::HD_COOKIE);

  std::vector<nghttp2_nv> nva;
  d.crumble_request_cookie(nva);

  auto num_cookies = d.count_crumble_request_cookie();

  assert_size(5, ==, nva.size());
  assert_size(5, ==, num_cookies);

  HeaderRefs cookies;
  std::transform(std::begin(nva), std::end(nva), std::back_inserter(cookies),
                 [](const nghttp2_nv &nv) {
                   return HeaderRef(StringRef{nv.name, nv.namelen},
                                    StringRef{nv.value, nv.valuelen},
                                    nv.flags & NGHTTP2_NV_FLAG_NO_INDEX);
                 });

  HeaderRefs ans = {{"cookie"_sr, "alpha"_sr},
                    {"cookie"_sr, "bravo"_sr},
                    {"cookie"_sr, "charlie"_sr},
                    {"cookie"_sr, "delta"_sr},
                    {"cookie"_sr, "echo"_sr}};

  assert_true(ans == cookies);
  assert_true(cookies[0].no_index);
  assert_true(cookies[1].no_index);
  assert_true(cookies[2].no_index);
}

void test_downstream_assemble_request_cookie(void) {
  Downstream d(nullptr, nullptr, 0);
  auto &req = d.request();

  req.fs.add_header_token(":method"_sr, "get"_sr, false, -1);
  req.fs.add_header_token(":path"_sr, "/"_sr, false, -1);
  req.fs.add_header_token("cookie"_sr, "alpha"_sr, false, http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "bravo;"_sr, false, http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "charlie; "_sr, false, http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "delta;;"_sr, false, http2::HD_COOKIE);
  assert_stdsv_equal("alpha; bravo; charlie; delta"sv,
                     d.assemble_request_cookie());
}

void test_downstream_rewrite_location_response_header(void) {
  Downstream d(nullptr, nullptr, 0);
  auto &req = d.request();
  auto &resp = d.response();
  d.set_request_downstream_host("localhost2"_sr);
  req.authority = "localhost:8443"_sr;
  resp.fs.add_header_token("location"_sr, "http://localhost2:3000/"_sr, false,
                           http2::HD_LOCATION);
  d.rewrite_location_response_header("https"_sr);
  auto location = resp.fs.header(http2::HD_LOCATION);
  assert_stdsv_equal("https://localhost:8443/"sv, (*location).value);
}

void test_downstream_supports_non_final_response(void) {
  Downstream d(nullptr, nullptr, 0);
  auto &req = d.request();

  req.http_major = 3;
  req.http_minor = 0;

  assert_true(d.supports_non_final_response());

  req.http_major = 2;
  req.http_minor = 0;

  assert_true(d.supports_non_final_response());

  req.http_major = 1;
  req.http_minor = 1;

  assert_true(d.supports_non_final_response());

  req.http_major = 1;
  req.http_minor = 0;

  assert_false(d.supports_non_final_response());

  req.http_major = 0;
  req.http_minor = 9;

  assert_false(d.supports_non_final_response());
}

void test_downstream_find_affinity_cookie(void) {
  Downstream d(nullptr, nullptr, 0);

  auto &req = d.request();
  req.fs.add_header_token("cookie"_sr, StringRef{}, false, http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "a=b;;c=d"_sr, false, http2::HD_COOKIE);
  req.fs.add_header_token("content-length"_sr, "599"_sr, false,
                          http2::HD_CONTENT_LENGTH);
  req.fs.add_header_token("cookie"_sr, "lb=deadbeef;LB=f1f2f3f4"_sr, false,
                          http2::HD_COOKIE);
  req.fs.add_header_token("cookie"_sr, "short=e1e2e3e"_sr, false,
                          http2::HD_COOKIE);

  uint32_t aff;

  aff = d.find_affinity_cookie("lb"_sr);

  assert_uint32(0xdeadbeef, ==, aff);

  aff = d.find_affinity_cookie("LB"_sr);

  assert_uint32(0xf1f2f3f4, ==, aff);

  aff = d.find_affinity_cookie("short"_sr);

  assert_uint32(0, ==, aff);
}

} // namespace shrpx
