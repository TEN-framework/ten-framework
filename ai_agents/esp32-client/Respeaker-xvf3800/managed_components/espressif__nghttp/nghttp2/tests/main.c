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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include "munit.h"

/* include test cases' include files here */
#include "nghttp2_pq_test.h"
#include "nghttp2_map_test.h"
#include "nghttp2_queue_test.h"
#include "nghttp2_session_test.h"
#include "nghttp2_frame_test.h"
#include "nghttp2_stream_test.h"
#include "nghttp2_hd_test.h"
#include "nghttp2_alpn_test.h"
#include "nghttp2_helper_test.h"
#include "nghttp2_buf_test.h"
#include "nghttp2_http_test.h"
#include "nghttp2_extpri_test.h"
#include "nghttp2_ratelim_test.h"

extern int nghttp2_enable_strict_preface;

int main(int argc, char *argv[]) {
  const MunitSuite suites[] = {
    pq_suite,
    map_suite,
    queue_suite,
    frame_suite,
    session_suite,
    hd_suite,
    alpn_suite,
    helper_suite,
    buf_suite,
    http_suite,
    extpri_suite,
    ratelim_suite,
    {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
  };
  const MunitSuite suite = {
    "", NULL, suites, 1, MUNIT_SUITE_OPTION_NONE,
  };

  nghttp2_enable_strict_preface = 0;

  return munit_suite_main(&suite, NULL, argc, argv);
}
