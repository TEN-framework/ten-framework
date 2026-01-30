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
#include "util_test.h"

#include <cstring>
#include <iostream>
#include <random>

#include "munitxx.h"

#include <nghttp2/nghttp2.h>

#include "util.h"
#include "template.h"

using namespace nghttp2;
using namespace std::literals;

namespace shrpx {

namespace {
const MunitTest tests[]{
  munit_void_test(test_util_streq),
  munit_void_test(test_util_strieq),
  munit_void_test(test_util_inp_strlower),
  munit_void_test(test_util_to_base64),
  munit_void_test(test_util_to_token68),
  munit_void_test(test_util_percent_encode_token),
  munit_void_test(test_util_percent_decode),
  munit_void_test(test_util_quote_string),
  munit_void_test(test_util_utox),
  munit_void_test(test_util_http_date),
  munit_void_test(test_util_select_h2),
  munit_void_test(test_util_ipv6_numeric_addr),
  munit_void_test(test_util_utos),
  munit_void_test(test_util_make_string_ref_uint),
  munit_void_test(test_util_utos_unit),
  munit_void_test(test_util_utos_funit),
  munit_void_test(test_util_parse_uint_with_unit),
  munit_void_test(test_util_parse_uint),
  munit_void_test(test_util_parse_duration_with_unit),
  munit_void_test(test_util_duration_str),
  munit_void_test(test_util_format_duration),
  munit_void_test(test_util_starts_with),
  munit_void_test(test_util_ends_with),
  munit_void_test(test_util_parse_http_date),
  munit_void_test(test_util_localtime_date),
  munit_void_test(test_util_get_uint64),
  munit_void_test(test_util_parse_config_str_list),
  munit_void_test(test_util_make_http_hostport),
  munit_void_test(test_util_make_hostport),
  munit_void_test(test_util_random_alpha_digit),
  munit_void_test(test_util_format_hex),
  munit_void_test(test_util_is_hex_string),
  munit_void_test(test_util_decode_hex),
  munit_void_test(test_util_extract_host),
  munit_void_test(test_util_split_hostport),
  munit_void_test(test_util_split_str),
  munit_void_test(test_util_rstrip),
  munit_test_end(),
};
} // namespace

const MunitSuite util_suite{
  "/util", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_util_streq(void) {
  assert_true(util::streq("alpha"_sr, "alpha"_sr, 5));
  assert_true(util::streq("alpha"_sr, "alphabravo"_sr, 5));
  assert_false(util::streq("alpha"_sr, "alphabravo"_sr, 6));
  assert_false(util::streq("alphabravo"_sr, "alpha"_sr, 5));
  assert_false(util::streq("alpha"_sr, "alphA"_sr, 5));
  assert_false(util::streq(""_sr, "a"_sr, 1));
  assert_true(util::streq(""_sr, ""_sr, 0));
  assert_false(util::streq("alpha"_sr, ""_sr, 0));
}

void test_util_strieq(void) {
  assert_true(util::strieq(std::string("alpha"), std::string("alpha")));
  assert_true(util::strieq(std::string("alpha"), std::string("AlPhA")));
  assert_true(util::strieq(std::string(), std::string()));
  assert_false(util::strieq(std::string("alpha"), std::string("AlPhA ")));
  assert_false(util::strieq(std::string(), std::string("AlPhA ")));

  assert_true(util::strieq("alpha"_sr, "alpha"_sr));
  assert_true(util::strieq("alpha"_sr, "AlPhA"_sr));
  assert_true(util::strieq(StringRef{}, StringRef{}));
  assert_false(util::strieq("alpha"_sr, "AlPhA "_sr));
  assert_false(util::strieq(""_sr, "AlPhA "_sr));

  assert_true(util::strieq("alpha"_sr, "alpha"_sr, 5));
  assert_true(util::strieq("alpha"_sr, "AlPhA"_sr, 5));
  assert_false(util::strieq("alpha"_sr, "AlPhA "_sr, 6));
  assert_false(util::strieq(""_sr, "AlPhA "_sr, 6));
}

void test_util_inp_strlower(void) {
  std::string a("alPha");
  util::inp_strlower(a);
  assert_stdstring_equal("alpha", a);

  a = "ALPHA123BRAVO";
  util::inp_strlower(a);
  assert_stdstring_equal("alpha123bravo", a);

  a = "";
  util::inp_strlower(a);
  assert_stdstring_equal("", a);
}

void test_util_to_base64(void) {
  BlockAllocator balloc(4096, 4096);

  assert_stdsv_equal("AAA++B/="sv, util::to_base64(balloc, "AAA--B_"_sr));
  assert_stdsv_equal("AAA++B/B"sv, util::to_base64(balloc, "AAA--B_B"_sr));
}

void test_util_to_token68(void) {
  std::string x = "AAA++B/=";
  util::to_token68(x);
  assert_stdstring_equal("AAA--B_", x);

  x = "AAA++B/B";
  util::to_token68(x);
  assert_stdstring_equal("AAA--B_B", x);
}

void test_util_percent_encode_token(void) {
  BlockAllocator balloc(4096, 4096);
  assert_stdsv_equal("h2"sv, util::percent_encode_token(balloc, "h2"_sr));
  assert_stdsv_equal("h3~"sv, util::percent_encode_token(balloc, "h3~"_sr));
  assert_stdsv_equal("100%25"sv, util::percent_encode_token(balloc, "100%"_sr));
  assert_stdsv_equal("http%202"sv,
                     util::percent_encode_token(balloc, "http 2"_sr));
}

void test_util_percent_decode(void) {
  {
    std::string s = "%66%6F%6f%62%61%72";
    assert_stdstring_equal("foobar",
                           util::percent_decode(std::begin(s), std::end(s)));
  }
  {
    std::string s = "%66%6";
    assert_stdstring_equal("f%6",
                           util::percent_decode(std::begin(s), std::end(s)));
  }
  {
    std::string s = "%66%";
    assert_stdstring_equal("f%",
                           util::percent_decode(std::begin(s), std::end(s)));
  }
  BlockAllocator balloc(1024, 1024);

  assert_stdsv_equal("foobar"sv,
                     util::percent_decode(balloc, "%66%6F%6f%62%61%72"_sr));

  assert_stdsv_equal("f%6"sv, util::percent_decode(balloc, "%66%6"_sr));

  assert_stdsv_equal("f%"sv, util::percent_decode(balloc, "%66%"_sr));
}

void test_util_quote_string(void) {
  BlockAllocator balloc(4096, 4096);
  assert_stdsv_equal("alpha"sv, util::quote_string(balloc, "alpha"_sr));
  assert_stdsv_equal(""sv, util::quote_string(balloc, ""_sr));
  assert_stdsv_equal("\\\"alpha\\\""sv,
                     util::quote_string(balloc, "\"alpha\""_sr));
}

void test_util_utox(void) {
  assert_stdstring_equal("0", util::utox(0));
  assert_stdstring_equal("1", util::utox(1));
  assert_stdstring_equal("F", util::utox(15));
  assert_stdstring_equal("10", util::utox(16));
  assert_stdstring_equal("3B9ACA07", util::utox(1000000007));
  assert_stdstring_equal("100000000", util::utox(1LL << 32));
}

void test_util_http_date(void) {
  assert_stdstring_equal("Thu, 01 Jan 1970 00:00:00 GMT", util::http_date(0));
  assert_stdstring_equal("Wed, 29 Feb 2012 09:15:16 GMT",
                         util::http_date(1330506916));

  std::array<char, 30> http_buf;

  assert_stdsv_equal(
    "Thu, 01 Jan 1970 00:00:00 GMT"sv,
    util::format_http_date(http_buf.data(),
                           std::chrono::system_clock::time_point()));
  assert_stdsv_equal("Wed, 29 Feb 2012 09:15:16 GMT"sv,
                     util::format_http_date(
                       http_buf.data(), std::chrono::system_clock::time_point(
                                          std::chrono::seconds(1330506916))));
}

void test_util_select_h2(void) {
  const unsigned char *out = nullptr;
  unsigned char outlen = 0;

  // Check single entry and select it.
  const unsigned char t1[] = "\x2h2";
  assert_true(util::select_h2(&out, &outlen, t1, sizeof(t1) - 1));
  assert_memory_equal(NGHTTP2_PROTO_VERSION_ID_LEN, NGHTTP2_PROTO_VERSION_ID,
                      out);
  assert_uchar(NGHTTP2_PROTO_VERSION_ID_LEN, ==, outlen);

  out = nullptr;
  outlen = 0;

  // Check the case where id is correct but length is invalid and too
  // long.
  const unsigned char t2[] = "\x6h2-14";
  assert_false(util::select_h2(&out, &outlen, t2, sizeof(t2) - 1));

  // Check the case where h2 is located after bogus ID.
  const unsigned char t3[] = "\x2h3\x2h2";
  assert_true(util::select_h2(&out, &outlen, t3, sizeof(t3) - 1));

  assert_memory_equal(NGHTTP2_PROTO_VERSION_ID_LEN, NGHTTP2_PROTO_VERSION_ID,
                      out);
  assert_uchar(NGHTTP2_PROTO_VERSION_ID_LEN, ==, outlen);

  out = nullptr;
  outlen = 0;

  // Check the case that last entry's length is invalid and too long.
  const unsigned char t4[] = "\x2h3\x6h2-14";
  assert_false(util::select_h2(&out, &outlen, t4, sizeof(t4) - 1));

  // Check the case that all entries are not supported.
  const unsigned char t5[] = "\x2h3\x2h4";
  assert_false(util::select_h2(&out, &outlen, t5, sizeof(t5) - 1));

  // Check the case where 2 values are eligible, but last one is
  // picked up because it has precedence over the other.
  const unsigned char t6[] = "\x5h2-14\x5h2-16";
  assert_true(util::select_h2(&out, &outlen, t6, sizeof(t6) - 1));
  assert_stdsv_equal(NGHTTP2_H2_16, (StringRef{out, outlen}));
}

void test_util_ipv6_numeric_addr(void) {
  assert_true(util::ipv6_numeric_addr("::1"));
  assert_true(
    util::ipv6_numeric_addr("2001:0db8:85a3:0042:1000:8a2e:0370:7334"));
  // IPv4
  assert_false(util::ipv6_numeric_addr("127.0.0.1"));
  // not numeric address
  assert_false(util::ipv6_numeric_addr("localhost"));
}

void test_util_utos(void) {
  uint8_t buf[32];

  assert_stdstring_equal("0", (std::string{buf, util::utos(buf, 0)}));
  assert_stdstring_equal("123", (std::string{buf, util::utos(buf, 123)}));
  assert_stdstring_equal(
    "18446744073709551615",
    (std::string{buf, util::utos(buf, 18446744073709551615ULL)}));
}

void test_util_make_string_ref_uint(void) {
  BlockAllocator balloc(1024, 1024);

  assert_stdsv_equal("0"sv, util::make_string_ref_uint(balloc, 0));
  assert_stdsv_equal("123"sv, util::make_string_ref_uint(balloc, 123));
  assert_stdsv_equal(
    "18446744073709551615"sv,
    util::make_string_ref_uint(balloc, 18446744073709551615ULL));
}

void test_util_utos_unit(void) {
  assert_stdstring_equal("0", util::utos_unit(0));
  assert_stdstring_equal("1023", util::utos_unit(1023));
  assert_stdstring_equal("1K", util::utos_unit(1024));
  assert_stdstring_equal("1K", util::utos_unit(1025));
  assert_stdstring_equal("1M", util::utos_unit(1 << 20));
  assert_stdstring_equal("1G", util::utos_unit(1 << 30));
  assert_stdstring_equal("1024G", util::utos_unit(1LL << 40));
}

void test_util_utos_funit(void) {
  assert_stdstring_equal("0", util::utos_funit(0));
  assert_stdstring_equal("1023", util::utos_funit(1023));
  assert_stdstring_equal("1.00K", util::utos_funit(1024));
  assert_stdstring_equal("1.00K", util::utos_funit(1025));
  assert_stdstring_equal("1.09K", util::utos_funit(1119));
  assert_stdstring_equal("1.27K", util::utos_funit(1300));
  assert_stdstring_equal("1.00M", util::utos_funit(1 << 20));
  assert_stdstring_equal("1.18M", util::utos_funit(1234567));
  assert_stdstring_equal("1.00G", util::utos_funit(1 << 30));
  assert_stdstring_equal("4492450797.23G",
                         util::utos_funit(4823732313248234343LL));
  assert_stdstring_equal("1024.00G", util::utos_funit(1LL << 40));
}

void test_util_parse_uint_with_unit(void) {
  assert_int64(0, ==, util::parse_uint_with_unit("0").value_or(-1));
  assert_int64(1023, ==, util::parse_uint_with_unit("1023").value_or(-1));
  assert_int64(1024, ==, util::parse_uint_with_unit("1k").value_or(-1));
  assert_int64(2048, ==, util::parse_uint_with_unit("2K").value_or(-1));
  assert_int64(1 << 20, ==, util::parse_uint_with_unit("1m").value_or(-1));
  assert_int64(1 << 21, ==, util::parse_uint_with_unit("2M").value_or(-1));
  assert_int64(1 << 30, ==, util::parse_uint_with_unit("1g").value_or(-1));
  assert_int64(1LL << 31, ==, util::parse_uint_with_unit("2G").value_or(-1));
  assert_int64(9223372036854775807LL, ==,
               util::parse_uint_with_unit("9223372036854775807").value_or(-1));
  // check overflow case
  assert_false(util::parse_uint_with_unit("9223372036854775808"));
  assert_false(util::parse_uint_with_unit("10000000000000000000"));
  assert_false(util::parse_uint_with_unit("9223372036854775807G"));
  // bad characters
  assert_false(util::parse_uint_with_unit("1.1"));
  assert_false(util::parse_uint_with_unit("1a"));
  assert_false(util::parse_uint_with_unit("a1"));
  assert_false(util::parse_uint_with_unit("1T"));
  assert_false(util::parse_uint_with_unit(""));
}

void test_util_parse_uint(void) {
  assert_int64(0, ==, util::parse_uint("0").value_or(-1));
  assert_int64(1023, ==, util::parse_uint("1023").value_or(-1));
  assert_false(util::parse_uint("1k"));
  assert_int64(9223372036854775807LL, ==,
               util::parse_uint("9223372036854775807").value_or(-1));
  // check overflow case
  assert_false(util::parse_uint("9223372036854775808"));
  assert_false(util::parse_uint("10000000000000000000"));
  // bad characters
  assert_false(util::parse_uint("1.1"));
  assert_false(util::parse_uint("1a"));
  assert_false(util::parse_uint("a1"));
  assert_false(util::parse_uint("1T"));
  assert_false(util::parse_uint(""));
}

void test_util_parse_duration_with_unit(void) {
  auto inf = std::numeric_limits<double>::infinity();

  assert_double(0., ==, util::parse_duration_with_unit("0").value_or(inf));
  assert_double(123., ==, util::parse_duration_with_unit("123").value_or(inf));
  assert_double(123., ==, util::parse_duration_with_unit("123s").value_or(inf));
  assert_double(0.500, ==,
                util::parse_duration_with_unit("500ms").value_or(inf));
  assert_double(123., ==, util::parse_duration_with_unit("123S").value_or(inf));
  assert_double(0.500, ==,
                util::parse_duration_with_unit("500MS").value_or(inf));
  assert_double(180, ==, util::parse_duration_with_unit("3m").value_or(inf));
  assert_double(3600 * 5, ==,
                util::parse_duration_with_unit("5h").value_or(inf));

  // check overflow case
  assert_false(util::parse_duration_with_unit("9223372036854775808"));
  // bad characters
  assert_false(util::parse_duration_with_unit("0u"));
  assert_false(util::parse_duration_with_unit("0xs"));
  assert_false(util::parse_duration_with_unit("0mt"));
  assert_false(util::parse_duration_with_unit("0mss"));
  assert_false(util::parse_duration_with_unit("s"));
  assert_false(util::parse_duration_with_unit("ms"));
}

void test_util_duration_str(void) {
  assert_stdstring_equal("0", util::duration_str(0.));
  assert_stdstring_equal("1s", util::duration_str(1.));
  assert_stdstring_equal("500ms", util::duration_str(0.5));
  assert_stdstring_equal("1500ms", util::duration_str(1.5));
  assert_stdstring_equal("2m", util::duration_str(120.));
  assert_stdstring_equal("121s", util::duration_str(121.));
  assert_stdstring_equal("1h", util::duration_str(3600.));
}

void test_util_format_duration(void) {
  assert_stdstring_equal("0us",
                         util::format_duration(std::chrono::microseconds(0)));
  assert_stdstring_equal("999us",
                         util::format_duration(std::chrono::microseconds(999)));
  assert_stdstring_equal(
    "1.00ms", util::format_duration(std::chrono::microseconds(1000)));
  assert_stdstring_equal(
    "1.09ms", util::format_duration(std::chrono::microseconds(1090)));
  assert_stdstring_equal(
    "1.01ms", util::format_duration(std::chrono::microseconds(1009)));
  assert_stdstring_equal(
    "999.99ms", util::format_duration(std::chrono::microseconds(999990)));
  assert_stdstring_equal(
    "1.00s", util::format_duration(std::chrono::microseconds(1000000)));
  assert_stdstring_equal(
    "1.05s", util::format_duration(std::chrono::microseconds(1050000)));

  assert_stdstring_equal("0us", util::format_duration(0.));
  assert_stdstring_equal("999us", util::format_duration(0.000999));
  assert_stdstring_equal("1.00ms", util::format_duration(0.001));
  assert_stdstring_equal("1.09ms", util::format_duration(0.00109));
  assert_stdstring_equal("1.01ms", util::format_duration(0.001009));
  assert_stdstring_equal("999.99ms", util::format_duration(0.99999));
  assert_stdstring_equal("1.00s", util::format_duration(1.));
  assert_stdstring_equal("1.05s", util::format_duration(1.05));
}

void test_util_starts_with(void) {
  assert_true(util::starts_with("foo"_sr, "foo"_sr));
  assert_true(util::starts_with("fooo"_sr, "foo"_sr));
  assert_true(util::starts_with("ofoo"_sr, StringRef{}));
  assert_false(util::starts_with("ofoo"_sr, "foo"_sr));

  assert_true(util::istarts_with("FOO"_sr, "fOO"_sr));
  assert_true(util::istarts_with("ofoo"_sr, StringRef{}));
  assert_true(util::istarts_with("fOOo"_sr, "Foo"_sr));
  assert_false(util::istarts_with("ofoo"_sr, "foo"_sr));
}

void test_util_ends_with(void) {
  assert_true(util::ends_with("foo"_sr, "foo"_sr));
  assert_true(util::ends_with("foo"_sr, StringRef{}));
  assert_true(util::ends_with("ofoo"_sr, "foo"_sr));
  assert_false(util::ends_with("ofoo"_sr, "fo"_sr));

  assert_true(util::iends_with("fOo"_sr, "Foo"_sr));
  assert_true(util::iends_with("foo"_sr, StringRef{}));
  assert_true(util::iends_with("oFoo"_sr, "fOO"_sr));
  assert_false(util::iends_with("ofoo"_sr, "fo"_sr));
}

void test_util_parse_http_date(void) {
  assert_int64(1001939696, ==,
               util::parse_http_date("Mon, 1 Oct 2001 12:34:56 GMT"_sr));
}

void test_util_localtime_date(void) {
  auto tz = getenv("TZ");
  if (tz) {
    tz = strdup(tz);
  }
#ifdef __linux__
  setenv("TZ", "NZST-12:00:00:00", 1);
#else  // !__linux__
  setenv("TZ", ":Pacific/Auckland", 1);
#endif // !__linux__
  tzset();

  assert_stdstring_equal("02/Oct/2001:00:34:56 +1200",
                         util::common_log_date(1001939696));
  assert_stdstring_equal("2001-10-02T00:34:56.123+12:00",
                         util::iso8601_date(1001939696000LL + 123));

  std::array<char, 27> common_buf;

  assert_stdsv_equal("02/Oct/2001:00:34:56 +1200"sv,
                     util::format_common_log(
                       common_buf.data(), std::chrono::system_clock::time_point(
                                            std::chrono::seconds(1001939696))));

  std::array<char, 30> iso8601_buf;

  assert_stdsv_equal(
    "2001-10-02T00:34:56.123+12:00"sv,
    util::format_iso8601(iso8601_buf.data(),
                         std::chrono::system_clock::time_point(
                           std::chrono::milliseconds(1001939696123LL))));

  if (tz) {
    setenv("TZ", tz, 1);
    free(tz);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

void test_util_get_uint64(void) {
  {
    auto v = std::to_array<unsigned char>(
      {0x01, 0x12, 0x34, 0x56, 0xff, 0x9a, 0xab, 0xbc});

    auto n = util::get_uint64(v.data());

    assert_uint64(0x01123456ff9aabbcULL, ==, n);
  }
  {
    auto v = std::to_array<unsigned char>(
      {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});

    auto n = util::get_uint64(v.data());

    assert_uint64(0xffffffffffffffffULL, ==, n);
  }
}

void test_util_parse_config_str_list(void) {
  auto res = util::parse_config_str_list("a"_sr);
  assert_size(1, ==, res.size());
  assert_stdstring_equal("a", res[0]);

  res = util::parse_config_str_list("a,"_sr);
  assert_size(2, ==, res.size());
  assert_stdstring_equal("a", res[0]);
  assert_stdstring_equal("", res[1]);

  res = util::parse_config_str_list(":a::"_sr, ':');
  assert_size(4, ==, res.size());
  assert_stdstring_equal("", res[0]);
  assert_stdstring_equal("a", res[1]);
  assert_stdstring_equal("", res[2]);
  assert_stdstring_equal("", res[3]);

  res = util::parse_config_str_list(StringRef{});
  assert_size(1, ==, res.size());
  assert_stdstring_equal("", res[0]);

  res = util::parse_config_str_list("alpha,bravo,charlie"_sr);
  assert_size(3, ==, res.size());
  assert_stdstring_equal("alpha", res[0]);
  assert_stdstring_equal("bravo", res[1]);
  assert_stdstring_equal("charlie", res[2]);
}

void test_util_make_http_hostport(void) {
  BlockAllocator balloc(4096, 4096);

  assert_stdsv_equal("localhost"sv,
                     util::make_http_hostport(balloc, "localhost"_sr, 80));
  assert_stdsv_equal("[::1]"sv,
                     util::make_http_hostport(balloc, "::1"_sr, 443));
  assert_stdsv_equal("localhost:3000"sv,
                     util::make_http_hostport(balloc, "localhost"_sr, 3000));
}

void test_util_make_hostport(void) {
  std::array<char, util::max_hostport> hostport_buf;
  assert_stdsv_equal(
    "localhost:80"sv,
    util::make_hostport(std::begin(hostport_buf), "localhost"_sr, 80));
  assert_stdsv_equal("[::1]:443"sv, util::make_hostport(
                                      std::begin(hostport_buf), "::1"_sr, 443));

  BlockAllocator balloc(4096, 4096);
  assert_stdsv_equal("localhost:80"sv,
                     util::make_hostport(balloc, "localhost"_sr, 80));
  assert_stdsv_equal("[::1]:443"sv, util::make_hostport(balloc, "::1"_sr, 443));
}

void test_util_random_alpha_digit(void) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::array<uint8_t, 19> data;

  auto p = util::random_alpha_digit(std::begin(data), std::end(data), gen);

  assert_true(std::end(data) == p);

  for (auto b : data) {
    assert_true(('A' <= b && b <= 'Z') || ('a' <= b && b <= 'z') ||
                ('0' <= b && b <= '9'));
  }
}

void test_util_format_hex(void) {
  BlockAllocator balloc(4096, 4096);

  assert_stdsv_equal("0ff0"sv,
                     util::format_hex(balloc, std::span{"\x0f\xf0"_sr}));
  assert_stdsv_equal(""sv,
                     util::format_hex(balloc, std::span<const uint8_t>{}));

  union T {
    uint16_t x;
    uint8_t y[2];
  };

  auto t = T{.y = {0xbe, 0xef}};

  assert_stdstring_equal("beef", util::format_hex(std::span{&t.x, 1}));

  std::string o;
  o.resize(4);

  assert_true(std::end(o) ==
              util::format_hex(std::begin(o), std::span{&t.x, 1}));
  assert_stdstring_equal("beef", o);

  struct S {
    uint8_t x[8];
  };

  auto s = S{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xf8}};

  assert_stdstring_equal("01020304050607f8", util::format_hex(s.x));
}

void test_util_is_hex_string(void) {
  assert_true(util::is_hex_string(StringRef{}));
  assert_true(util::is_hex_string("0123456789abcdef"_sr));
  assert_true(util::is_hex_string("0123456789ABCDEF"_sr));
  assert_false(util::is_hex_string("000"_sr));
  assert_false(util::is_hex_string("XX"_sr));
}

void test_util_decode_hex(void) {
  BlockAllocator balloc(4096, 4096);

  assert_stdsv_equal("\x0f\xf0"sv,
                     StringRef{util::decode_hex(balloc, "0ff0"_sr)});
  assert_stdsv_equal(""sv, StringRef{util::decode_hex(balloc, StringRef{})});
}

void test_util_extract_host(void) {
  assert_stdsv_equal("foo"sv, util::extract_host("foo"_sr));
  assert_stdsv_equal("foo"sv, util::extract_host("foo:"_sr));
  assert_stdsv_equal("foo"sv, util::extract_host("foo:0"_sr));
  assert_stdsv_equal("[::1]"sv, util::extract_host("[::1]"_sr));
  assert_stdsv_equal("[::1]"sv, util::extract_host("[::1]:"_sr));

  assert_true(util::extract_host(":foo"_sr).empty());
  assert_true(util::extract_host("[::1"_sr).empty());
  assert_true(util::extract_host("[::1]0"_sr).empty());
  assert_true(util::extract_host(StringRef{}).empty());
}

void test_util_split_hostport(void) {
  assert_true(std::make_pair("foo"_sr, StringRef{}) ==
              util::split_hostport("foo"_sr));
  assert_true(std::make_pair("foo"_sr, "80"_sr) ==
              util::split_hostport("foo:80"_sr));
  assert_true(std::make_pair("::1"_sr, "80"_sr) ==
              util::split_hostport("[::1]:80"_sr));
  assert_true(std::make_pair("::1"_sr, StringRef{}) ==
              util::split_hostport("[::1]"_sr));

  assert_true(std::make_pair(StringRef{}, StringRef{}) ==
              util::split_hostport(StringRef{}));
  assert_true(std::make_pair(StringRef{}, StringRef{}) ==
              util::split_hostport("[::1]:"_sr));
  assert_true(std::make_pair(StringRef{}, StringRef{}) ==
              util::split_hostport("foo:"_sr));
  assert_true(std::make_pair(StringRef{}, StringRef{}) ==
              util::split_hostport("[::1:"_sr));
  assert_true(std::make_pair(StringRef{}, StringRef{}) ==
              util::split_hostport("[::1]80"_sr));
}

void test_util_split_str(void) {
  assert_true(std::vector<StringRef>{""_sr} == util::split_str(""_sr, ','));
  assert_true(std::vector<StringRef>{"alpha"_sr} ==
              util::split_str("alpha"_sr, ','));
  assert_true((std::vector<StringRef>{"alpha"_sr, ""_sr}) ==
              util::split_str("alpha,"_sr, ','));
  assert_true((std::vector<StringRef>{"alpha"_sr, "bravo"_sr}) ==
              util::split_str("alpha,bravo"_sr, ','));
  assert_true((std::vector<StringRef>{"alpha"_sr, "bravo"_sr, "charlie"_sr}) ==
              util::split_str("alpha,bravo,charlie"_sr, ','));
  assert_true((std::vector<StringRef>{"alpha"_sr, "bravo"_sr, "charlie"_sr}) ==
              util::split_str("alpha,bravo,charlie"_sr, ',', 0));
  assert_true(std::vector<StringRef>{""_sr} == util::split_str(""_sr, ',', 1));
  assert_true(std::vector<StringRef>{""_sr} == util::split_str(""_sr, ',', 2));
  assert_true((std::vector<StringRef>{"alpha"_sr, "bravo,charlie"_sr}) ==
              util::split_str("alpha,bravo,charlie"_sr, ',', 2));
  assert_true(std::vector<StringRef>{"alpha"_sr} ==
              util::split_str("alpha"_sr, ',', 2));
  assert_true((std::vector<StringRef>{"alpha"_sr, ""_sr}) ==
              util::split_str("alpha,"_sr, ',', 2));
  assert_true(std::vector<StringRef>{"alpha"_sr} ==
              util::split_str("alpha"_sr, ',', 0));
  assert_true(std::vector<StringRef>{"alpha,bravo,charlie"_sr} ==
              util::split_str("alpha,bravo,charlie"_sr, ',', 1));
}

void test_util_rstrip(void) {
  BlockAllocator balloc(4096, 4096);

  assert_stdsv_equal("alpha"sv, util::rstrip(balloc, "alpha"_sr));
  assert_stdsv_equal("alpha"sv, util::rstrip(balloc, "alpha "_sr));
  assert_stdsv_equal("alpha"sv, util::rstrip(balloc, "alpha \t"_sr));
  assert_stdsv_equal(""sv, util::rstrip(balloc, ""_sr));
  assert_stdsv_equal(""sv, util::rstrip(balloc, "\t\t\t   "_sr));
}

} // namespace shrpx
