/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
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
#include "base64_test.h"

#include <cstring>
#include <iostream>

#include "munitxx.h"

#include <nghttp2/nghttp2.h>

#include "base64.h"

using namespace std::literals;

namespace nghttp2 {

namespace {
const MunitTest tests[]{
  munit_void_test(test_base64_encode),
  munit_void_test(test_base64_decode),
  munit_test_end(),
};
} // namespace

const MunitSuite base64_suite{
  "/base64", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_base64_encode(void) {
  {
    std::string in = "\xff";
    auto out = base64::encode(std::begin(in), std::end(in));
    assert_stdstring_equal("/w==", out);
  }
  {
    std::string in = "\xff\xfe";
    auto out = base64::encode(std::begin(in), std::end(in));
    assert_stdstring_equal("//4=", out);
  }
  {
    std::string in = "\xff\xfe\xfd";
    auto out = base64::encode(std::begin(in), std::end(in));
    assert_stdstring_equal("//79", out);
  }
  {
    std::string in = "\xff\xfe\xfd\xfc";
    auto out = base64::encode(std::begin(in), std::end(in));
    assert_stdstring_equal("//79/A==", out);
  }
}

void test_base64_decode(void) {
  BlockAllocator balloc(4096, 4096);
  {
    auto in = "/w=="sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal("\xff"sv, out);
    assert_stdsv_equal("\xff"sv, StringRef{base64::decode(
                                   balloc, std::begin(in), std::end(in))});
  }
  {
    auto in = "//4="sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal("\xff\xfe"sv, out);
    assert_stdsv_equal("\xff\xfe"sv, StringRef{base64::decode(
                                       balloc, std::begin(in), std::end(in))});
  }
  {
    auto in = "//79"sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal("\xff\xfe\xfd"sv, out);
    assert_stdsv_equal(
      "\xff\xfe\xfd"sv,
      StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
  {
    auto in = "//79/A=="sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal("\xff\xfe\xfd\xfc"sv, out);
    assert_stdsv_equal(
      "\xff\xfe\xfd\xfc"sv,
      StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
  {
    // we check the number of valid input must be multiples of 4
    auto in = "//79="sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal(""sv, out);
    assert_stdsv_equal(
      ""sv, StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
  {
    // ending invalid character at the boundary of multiples of 4 is
    // bad
    auto in = "bmdodHRw\n"sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal("", out);
    assert_stdsv_equal(
      ""sv, StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
  {
    // after seeing '=', subsequent input must be also '='.
    auto in = "//79/A=A"sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal(""sv, out);
    assert_stdsv_equal(
      ""sv, StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
  {
    // additional '=' at the end is bad
    auto in = "//79/A======"sv;
    auto out = base64::decode(std::begin(in), std::end(in));
    assert_stdsv_equal(""sv, out);
    assert_stdsv_equal(
      ""sv, StringRef{base64::decode(balloc, std::begin(in), std::end(in))});
  }
}

} // namespace nghttp2
