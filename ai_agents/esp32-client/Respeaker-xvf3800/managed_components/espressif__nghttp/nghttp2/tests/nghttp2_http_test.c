/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2022 nghttp3 contributors
 * Copyright (c) 2022 nghttp2 contributors
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
#include "nghttp2_http_test.h"

#include <stdio.h>
#include <assert.h>

#include "munit.h"

#include "nghttp2_http.h"
#include "nghttp2_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp2_http_parse_priority),
  munit_test_end(),
};

const MunitSuite http_suite = {
  "/http", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp2_http_parse_priority(void) {
  int rv;

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)-1, ==, pri.urgency);
    assert_int(-1, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=7,i";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)7, ==, pri.urgency);
    assert_int(1, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0,i=?0";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)0, ==, pri.urgency);
    assert_int(0, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=3, i";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)3, ==, pri.urgency);
    assert_int(1, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0, i, i=?0, u=6";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)6, ==, pri.urgency);
    assert_int(0, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0,";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0, ";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?1";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)-1, ==, pri.urgency);
    assert_int(1, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?2";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=-1";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=8";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] =
      "i=?0, u=1, a=(x y z), u=2; i=?0;foo=\",,,\", i=?1;i=?0; u=6";

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int(0, ==, rv);
    assert_uint32((uint32_t)2, ==, pri.urgency);
    assert_int(1, ==, pri.inc);
  }

  {
    nghttp2_extpri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = {'u', '='};

    rv = nghttp2_http_parse_priority(&pri, v, sizeof(v));

    assert_int(NGHTTP2_ERR_INVALID_ARGUMENT, ==, rv);
  }
}
