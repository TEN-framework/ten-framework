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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#include "munit.h"

// include test cases' include files here
#include "shrpx_tls_test.h"
#include "shrpx_downstream_test.h"
#include "shrpx_config_test.h"
#include "shrpx_worker_test.h"
#include "http2_test.h"
#include "util_test.h"
#include "nghttp2_gzip_test.h"
#include "buffer_test.h"
#include "memchunk_test.h"
#include "template_test.h"
#include "shrpx_http_test.h"
#include "base64_test.h"
#include "shrpx_config.h"
#include "tls.h"
#include "shrpx_router_test.h"
#include "shrpx_log.h"
#ifdef ENABLE_HTTP3
#  include "siphash_test.h"
#endif // ENABLE_HTTP3

int main(int argc, char *argv[]) {
  shrpx::create_config();

  const MunitSuite suites[] = {
    shrpx::tls_suite,
    shrpx::downstream_suite,
    shrpx::config_suite,
    shrpx::worker_suite,
    shrpx::http_suite,
    shrpx::router_suite,
    shrpx::http2_suite,
    shrpx::util_suite,
    gzip_suite,
    buffer_suite,
    memchunk_suite,
    template_suite,
    base64_suite,
#ifdef ENABLE_HTTP3
    siphash_suite,
#endif // ENABLE_HTTP3
    {},
  };
  const MunitSuite suite = {
    "", nullptr, suites, 1, MUNIT_SUITE_OPTION_NONE,
  };

  return munit_suite_main(&suite, nullptr, argc, argv);
}
