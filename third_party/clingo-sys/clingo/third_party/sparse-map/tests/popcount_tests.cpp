/**
 * MIT License
 *
 * Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <tsl/sparse_hash.h>

#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <limits>

BOOST_AUTO_TEST_SUITE(test_popcount)

BOOST_AUTO_TEST_CASE(test_popcount_1) {
  std::uint32_t value = 0;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcount(value), 0);

  value = 1;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcount(value), 1);

  value = 2;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcount(value), 1);

  value = 294967496;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcount(value), 12);

  value = std::numeric_limits<std::uint32_t>::max();
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcount(value), 32);
}

BOOST_AUTO_TEST_CASE(test_popcountll_1) {
  std::uint64_t value = 0;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 0);

  value = 1;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 1);

  value = 2;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 1);

  value = 294967496;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 12);

  value = 8446744073709551416ull;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 40);

  value = std::numeric_limits<std::uint64_t>::max();
  BOOST_CHECK_EQUAL(tsl::detail_popcount::popcountll(value), 64);
}

BOOST_AUTO_TEST_CASE(test_fallback_popcount_1) {
  std::uint32_t value = 0;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcount(value), 0);

  value = 1;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcount(value), 1);

  value = 2;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcount(value), 1);

  value = 294967496;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcount(value), 12);

  value = std::numeric_limits<std::uint32_t>::max();
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcount(value), 32);
}

BOOST_AUTO_TEST_CASE(test_fallback_popcountll_1) {
  std::uint64_t value = 0;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 0);

  value = 1;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 1);

  value = 2;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 1);

  value = 294967496;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 12);

  value = 8446744073709551416ull;
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 40);

  value = std::numeric_limits<std::uint64_t>::max();
  BOOST_CHECK_EQUAL(tsl::detail_popcount::fallback_popcountll(value), 64);
}

BOOST_AUTO_TEST_SUITE_END()
