/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#include "shrpx_config_test.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H

#include <cstdlib>

#include "munitxx.h"

#include "shrpx_config.h"
#include "shrpx_log.h"

using namespace std::literals;

namespace shrpx {

namespace {
const MunitTest tests[]{
  munit_void_test(test_shrpx_config_parse_header),
  munit_void_test(test_shrpx_config_parse_log_format),
  munit_void_test(test_shrpx_config_read_tls_ticket_key_file),
  munit_void_test(test_shrpx_config_read_tls_ticket_key_file_aes_256),
  munit_test_end(),
};
} // namespace

const MunitSuite config_suite{
  "/config_suite", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_shrpx_config_parse_header(void) {
  BlockAllocator balloc(4096, 4096);

  auto p = parse_header(balloc, "a: b"_sr);
  assert_stdsv_equal("a"sv, p.name);
  assert_stdsv_equal("b"sv, p.value);

  p = parse_header(balloc, "a:  b"_sr);
  assert_stdsv_equal("a"sv, p.name);
  assert_stdsv_equal("b"sv, p.value);

  p = parse_header(balloc, ":a: b"_sr);
  assert_true(p.name.empty());

  p = parse_header(balloc, "a: :b"_sr);
  assert_stdsv_equal("a"sv, p.name);
  assert_stdsv_equal(":b"sv, p.value);

  p = parse_header(balloc, ": b"_sr);
  assert_true(p.name.empty());

  p = parse_header(balloc, "alpha: bravo charlie"_sr);
  assert_stdsv_equal("alpha", p.name);
  assert_stdsv_equal("bravo charlie", p.value);

  p = parse_header(balloc, "a,: b"_sr);
  assert_true(p.name.empty());

  p = parse_header(balloc, "a: b\x0a"_sr);
  assert_true(p.name.empty());
}

void test_shrpx_config_parse_log_format(void) {
  BlockAllocator balloc(4096, 4096);

  auto res = parse_log_format(
    balloc, R"($remote_addr - $remote_user [$time_local] )"
            R"("$request" $status $body_bytes_sent )"
            R"("${http_referer}" $http_host "$http_user_agent")"_sr);
  assert_size(16, ==, res.size());

  assert_enum_class(LogFragmentType::REMOTE_ADDR, ==, res[0].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[1].type);
  assert_stdsv_equal(" - $remote_user ["sv, res[1].value);

  assert_enum_class(LogFragmentType::TIME_LOCAL, ==, res[2].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[3].type);
  assert_stdsv_equal("] \""sv, res[3].value);

  assert_enum_class(LogFragmentType::REQUEST, ==, res[4].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[5].type);
  assert_stdsv_equal("\" "sv, res[5].value);

  assert_enum_class(LogFragmentType::STATUS, ==, res[6].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[7].type);
  assert_stdsv_equal(" "sv, res[7].value);

  assert_enum_class(LogFragmentType::BODY_BYTES_SENT, ==, res[8].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[9].type);
  assert_stdsv_equal(" \""sv, res[9].value);

  assert_enum_class(LogFragmentType::HTTP, ==, res[10].type);
  assert_stdsv_equal("referer"sv, res[10].value);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[11].type);
  assert_stdsv_equal("\" "sv, res[11].value);

  assert_enum_class(LogFragmentType::AUTHORITY, ==, res[12].type);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[13].type);
  assert_stdsv_equal(" \""sv, res[13].value);

  assert_enum_class(LogFragmentType::HTTP, ==, res[14].type);
  assert_stdsv_equal("user-agent"sv, res[14].value);

  assert_enum_class(LogFragmentType::LITERAL, ==, res[15].type);
  assert_stdsv_equal("\""sv, res[15].value);

  res = parse_log_format(balloc, "$"_sr);

  assert_size(1, ==, res.size());

  assert_enum_class(LogFragmentType::LITERAL, ==, res[0].type);
  assert_stdsv_equal("$"sv, res[0].value);

  res = parse_log_format(balloc, "${"_sr);

  assert_size(1, ==, res.size());

  assert_enum_class(LogFragmentType::LITERAL, ==, res[0].type);
  assert_stdsv_equal("${"sv, res[0].value);

  res = parse_log_format(balloc, "${a"_sr);

  assert_size(1, ==, res.size());

  assert_enum_class(LogFragmentType::LITERAL, ==, res[0].type);
  assert_stdsv_equal("${a"sv, res[0].value);

  res = parse_log_format(balloc, "${a "_sr);

  assert_size(1, ==, res.size());

  assert_enum_class(LogFragmentType::LITERAL, ==, res[0].type);
  assert_stdsv_equal("${a "sv, res[0].value);

  res = parse_log_format(balloc, "$$remote_addr"_sr);

  assert_size(2, ==, res.size());

  assert_enum_class(LogFragmentType::LITERAL, ==, res[0].type);
  assert_stdsv_equal("$"sv, res[0].value);

  assert_enum_class(LogFragmentType::REMOTE_ADDR, ==, res[1].type);
  assert_stdsv_equal(""sv, res[1].value);
}

void test_shrpx_config_read_tls_ticket_key_file(void) {
  char file1[] = "/tmp/nghttpx-unittest.XXXXXX";
  auto fd1 = mkstemp(file1);
  assert_int(-1, !=, fd1);
  assert_ssize(
    48, ==, write(fd1, "0..............12..............34..............5", 48));
  char file2[] = "/tmp/nghttpx-unittest.XXXXXX";
  auto fd2 = mkstemp(file2);
  assert_int(-1, !=, fd2);
  assert_ssize(
    48, ==, write(fd2, "6..............78..............9a..............b", 48));

  close(fd1);
  close(fd2);
  auto ticket_keys = read_tls_ticket_key_file(
    {StringRef{file1}, StringRef{file2}}, EVP_aes_128_cbc(), EVP_sha256());
  unlink(file1);
  unlink(file2);
  assert_not_null(ticket_keys.get());
  assert_size(2, ==, ticket_keys->keys.size());
  auto key = &ticket_keys->keys[0];
  assert_true(std::equal(std::begin(key->data.name), std::end(key->data.name),
                         "0..............1"));
  assert_true(std::equal(std::begin(key->data.enc_key),
                         std::begin(key->data.enc_key) + 16,
                         "2..............3"));
  assert_true(std::equal(std::begin(key->data.hmac_key),
                         std::begin(key->data.hmac_key) + 16,
                         "4..............5"));
  assert_size(16, ==, key->hmac_keylen);

  key = &ticket_keys->keys[1];
  assert_true(std::equal(std::begin(key->data.name), std::end(key->data.name),
                         "6..............7"));
  assert_true(std::equal(std::begin(key->data.enc_key),
                         std::begin(key->data.enc_key) + 16,
                         "8..............9"));
  assert_true(std::equal(std::begin(key->data.hmac_key),
                         std::begin(key->data.hmac_key) + 16,
                         "a..............b"));
  assert_size(16, ==, key->hmac_keylen);
}

void test_shrpx_config_read_tls_ticket_key_file_aes_256(void) {
  char file1[] = "/tmp/nghttpx-unittest.XXXXXX";
  auto fd1 = mkstemp(file1);
  assert_int(-1, !=, fd1);
  assert_ssize(80, ==,
               write(fd1,
                     "0..............12..............................34..."
                     "...........................5",
                     80));
  char file2[] = "/tmp/nghttpx-unittest.XXXXXX";
  auto fd2 = mkstemp(file2);
  assert_int(-1, !=, fd2);
  assert_ssize(80, ==,
               write(fd2,
                     "6..............78..............................9a..."
                     "...........................b",
                     80));

  close(fd1);
  close(fd2);
  auto ticket_keys = read_tls_ticket_key_file(
    {StringRef{file1}, StringRef{file2}}, EVP_aes_256_cbc(), EVP_sha256());
  unlink(file1);
  unlink(file2);
  assert_not_null(ticket_keys.get());
  assert_size(2, ==, ticket_keys->keys.size());
  auto key = &ticket_keys->keys[0];
  assert_true(std::equal(std::begin(key->data.name), std::end(key->data.name),
                         "0..............1"));
  assert_true(std::equal(std::begin(key->data.enc_key),
                         std::end(key->data.enc_key),
                         "2..............................3"));
  assert_true(std::equal(std::begin(key->data.hmac_key),
                         std::end(key->data.hmac_key),
                         "4..............................5"));

  key = &ticket_keys->keys[1];
  assert_true(std::equal(std::begin(key->data.name), std::end(key->data.name),
                         "6..............7"));
  assert_true(std::equal(std::begin(key->data.enc_key),
                         std::end(key->data.enc_key),
                         "8..............................9"));
  assert_true(std::equal(std::begin(key->data.hmac_key),
                         std::end(key->data.hmac_key),
                         "a..............................b"));
}

} // namespace shrpx
