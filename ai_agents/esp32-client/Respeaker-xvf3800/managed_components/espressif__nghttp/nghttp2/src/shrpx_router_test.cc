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
#include "shrpx_router_test.h"

#include "munitxx.h"

#include "shrpx_router.h"

namespace shrpx {

namespace {
const MunitTest tests[]{
  munit_void_test(test_shrpx_router_match),
  munit_void_test(test_shrpx_router_match_wildcard),
  munit_void_test(test_shrpx_router_match_prefix),
  munit_test_end(),
};
} // namespace

const MunitSuite router_suite{
  "/router", tests, nullptr, 1, MUNIT_SUITE_OPTION_NONE,
};

struct Pattern {
  StringRef pattern;
  size_t idx;
  bool wildcard;
};

void test_shrpx_router_match(void) {
  auto patterns = std::vector<Pattern>{
    {"nghttp2.org/"_sr, 0},
    {"nghttp2.org/alpha"_sr, 1},
    {"nghttp2.org/alpha/"_sr, 2},
    {"nghttp2.org/alpha/bravo/"_sr, 3},
    {"www.nghttp2.org/alpha/"_sr, 4},
    {"/alpha"_sr, 5},
    {"example.com/alpha/"_sr, 6},
    {"nghttp2.org/alpha/bravo2/"_sr, 7},
    {"www2.nghttp2.org/alpha/"_sr, 8},
    {"www2.nghttp2.org/alpha2/"_sr, 9},
  };

  Router router;

  for (auto &p : patterns) {
    router.add_route(p.pattern, p.idx);
  }

  ssize_t idx;

  idx = router.match("nghttp2.org"_sr, "/"_sr);

  assert_ssize(0, ==, idx);

  idx = router.match("nghttp2.org"_sr, "/alpha"_sr);

  assert_ssize(1, ==, idx);

  idx = router.match("nghttp2.org"_sr, "/alpha/"_sr);

  assert_ssize(2, ==, idx);

  idx = router.match("nghttp2.org"_sr, "/alpha/charlie"_sr);

  assert_ssize(2, ==, idx);

  idx = router.match("nghttp2.org"_sr, "/alpha/bravo/"_sr);

  assert_ssize(3, ==, idx);

  // matches pattern when last '/' is missing in path
  idx = router.match("nghttp2.org"_sr, "/alpha/bravo"_sr);

  assert_ssize(3, ==, idx);

  idx = router.match("www2.nghttp2.org"_sr, "/alpha"_sr);

  assert_ssize(8, ==, idx);

  idx = router.match(StringRef{}, "/alpha"_sr);

  assert_ssize(5, ==, idx);
}

void test_shrpx_router_match_wildcard(void) {
  constexpr auto patterns = std::to_array<Pattern>({
    {"nghttp2.org/"_sr, 0},
    {"nghttp2.org/"_sr, 1, true},
    {"nghttp2.org/alpha/"_sr, 2},
    {"nghttp2.org/alpha/"_sr, 3, true},
    {"nghttp2.org/bravo"_sr, 4},
    {"nghttp2.org/bravo"_sr, 5, true},
  });

  Router router;

  for (auto &p : patterns) {
    router.add_route(p.pattern, p.idx, p.wildcard);
  }

  assert_ssize(0, ==, router.match("nghttp2.org"_sr, "/"_sr));

  assert_ssize(1, ==, router.match("nghttp2.org"_sr, "/a"_sr));

  assert_ssize(1, ==, router.match("nghttp2.org"_sr, "/charlie"_sr));

  assert_ssize(2, ==, router.match("nghttp2.org"_sr, "/alpha"_sr));

  assert_ssize(2, ==, router.match("nghttp2.org"_sr, "/alpha/"_sr));

  assert_ssize(3, ==, router.match("nghttp2.org"_sr, "/alpha/b"_sr));

  assert_ssize(4, ==, router.match("nghttp2.org"_sr, "/bravo"_sr));

  assert_ssize(5, ==, router.match("nghttp2.org"_sr, "/bravocharlie"_sr));

  assert_ssize(5, ==, router.match("nghttp2.org"_sr, "/bravo/"_sr));
}

void test_shrpx_router_match_prefix(void) {
  auto patterns = std::vector<Pattern>{
    {"gro.2ptthgn."_sr, 0},
    {"gro.2ptthgn.www."_sr, 1},
    {"gro.2ptthgn.gmi."_sr, 2},
    {"gro.2ptthgn.gmi.ahpla."_sr, 3},
  };

  Router router;

  for (auto &p : patterns) {
    router.add_route(p.pattern, p.idx);
  }

  ssize_t idx;
  const RNode *node;
  size_t nread;

  node = nullptr;

  idx = router.match_prefix(&nread, &node, "gro.2ptthgn.gmi.ahpla.ovarb"_sr);

  assert_ssize(0, ==, idx);
  assert_size(12, ==, nread);

  idx = router.match_prefix(&nread, &node, "gmi.ahpla.ovarb"_sr);

  assert_ssize(2, ==, idx);
  assert_size(4, ==, nread);

  idx = router.match_prefix(&nread, &node, "ahpla.ovarb"_sr);

  assert_ssize(3, ==, idx);
  assert_ssize(6, ==, nread);
}

} // namespace shrpx
