/**
 * MIT License
 *
 * Copyright (c) 2018 Thibaut Goetghebuer-Planchon <tessil@gmx.com>
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
#include <tsl/bhopscotch_set.h>
#include <tsl/hopscotch_set.h>

#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <utility>

#include "utils.h"

BOOST_AUTO_TEST_SUITE(test_hopscotch_set)

using test_types = boost::mpl::list<
    tsl::hopscotch_set<std::int64_t, mod_hash<9>>,
    tsl::hopscotch_set<self_reference_member_test, mod_hash<9>>,
    tsl::hopscotch_set<move_only_test, mod_hash<9>>,
    tsl::hopscotch_pg_set<move_only_test, mod_hash<9>>,
    tsl::bhopscotch_set<std::int64_t, mod_hash<9>>,
    tsl::bhopscotch_set<self_reference_member_test, mod_hash<9>>,
    tsl::bhopscotch_set<move_only_test, mod_hash<9>>,
    tsl::bhopscotch_pg_set<move_only_test, mod_hash<9>>>;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HSet, test_types) {
  // insert x values, insert them again, check values
  using key_t = typename HSet::key_type;

  const std::size_t nb_values = 1000;
  HSet set;
  typename HSet::iterator it;
  bool inserted;

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) = set.insert(utils::get_key<key_t>(i));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(set.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) = set.insert(utils::get_key<key_t>(i));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(i));
    BOOST_CHECK(!inserted);
  }

  for (std::size_t i = 0; i < nb_values; i++) {
    it = set.find(utils::get_key<key_t>(i));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(i));
  }
}

BOOST_AUTO_TEST_CASE(test_compare) {
  const tsl::hopscotch_set<std::string> set1_1 = {"a", "e", "d", "c", "b"};
  const tsl::hopscotch_set<std::string> set1_2 = {"e", "c", "b", "a", "d"};
  const tsl::hopscotch_set<std::string> set2_1 = {"e", "c", "b", "a", "d", "f"};
  const tsl::hopscotch_set<std::string> set3_1 = {"e", "c", "b", "a", "aa"};
  const tsl::hopscotch_set<std::string> set4_1 = {};
  const tsl::hopscotch_set<std::string> set4_2 = {};

  BOOST_CHECK(set1_1 == set1_2);
  BOOST_CHECK(set1_2 == set1_1);

  BOOST_CHECK(set4_1 == set4_2);
  BOOST_CHECK(set4_2 == set4_1);

  BOOST_CHECK(set1_1 != set2_1);
  BOOST_CHECK(set2_1 != set1_1);

  BOOST_CHECK(set1_1 != set4_1);
  BOOST_CHECK(set4_1 != set1_1);

  BOOST_CHECK(set1_1 != set3_1);
  BOOST_CHECK(set3_1 != set1_1);

  BOOST_CHECK(set2_1 != set3_1);
  BOOST_CHECK(set3_1 != set2_1);
}

BOOST_AUTO_TEST_CASE(test_insert_pointer) {
  // Test added mainly to be sure that the code compiles with MSVC
  std::string value;
  std::string* value_ptr = &value;

  tsl::hopscotch_set<std::string*> set;
  set.insert(value_ptr);
  set.emplace(value_ptr);

  BOOST_CHECK_EQUAL(set.size(), 1);
  BOOST_CHECK_EQUAL(**set.begin(), value);
}

BOOST_AUTO_TEST_SUITE_END()
