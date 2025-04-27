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
#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "tsl/ordered_set.h"
#include "utils.h"

BOOST_AUTO_TEST_SUITE(test_ordered_set)

using test_types = boost::mpl::list<
    tsl::ordered_set<std::int64_t>,
    tsl::ordered_set<std::int64_t, std::hash<std::int64_t>,
                     std::equal_to<std::int64_t>, std::allocator<std::int64_t>,
                     std::vector<std::int64_t>>,
    tsl::ordered_set<std::int64_t, mod_hash<9>>, tsl::ordered_set<std::string>,
    tsl::ordered_set<std::string, mod_hash<9>>,
    tsl::ordered_set<move_only_test, mod_hash<9>>>;

/**
 * insert
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HSet, test_types) {
  // insert x values, insert them again, check values through find, check order
  // through iterator
  using key_t = typename HSet::key_type;

  const std::size_t nb_values = 1000;

  HSet set;
  typename HSet::iterator it;
  bool inserted;

  for (std::size_t i = 0; i < nb_values; i++) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    std::tie(it, inserted) = set.insert(utils::get_key<key_t>(insert_val));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(set.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    std::tie(it, inserted) = set.insert(utils::get_key<key_t>(insert_val));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
    BOOST_CHECK(!inserted);
  }
  BOOST_CHECK_EQUAL(set.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    it = set.find(utils::get_key<key_t>(insert_val));

    BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
  }

  std::size_t i = 0;
  for (const auto& value : set) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;

    BOOST_CHECK_EQUAL(value, utils::get_key<key_t>(insert_val));

    i++;
  }
}

BOOST_AUTO_TEST_CASE(test_compare) {
  const tsl::ordered_set<std::string> map = {"D", "L", "A"};

  BOOST_ASSERT(map == (tsl::ordered_set<std::string>{"D", "L", "A"}));
  BOOST_ASSERT(map != (tsl::ordered_set<std::string>{"L", "D", "A"}));

  BOOST_ASSERT(map < (tsl::ordered_set<std::string>{"D", "L", "B"}));
  BOOST_ASSERT(map <= (tsl::ordered_set<std::string>{"D", "L", "B"}));
  BOOST_ASSERT(map <= (tsl::ordered_set<std::string>{"D", "L", "A"}));

  BOOST_ASSERT(map > (tsl::ordered_set<std::string>{"D", {"K", 2}, "A"}));
  BOOST_ASSERT(map >= (tsl::ordered_set<std::string>{"D", {"K", 2}, "A"}));
  BOOST_ASSERT(map >= (tsl::ordered_set<std::string>{"D", "L", "A"}));
}

BOOST_AUTO_TEST_CASE(test_insert_pointer) {
  // Test added mainly to be sure that the code compiles with MSVC
  std::string value;
  std::string* value_ptr = &value;

  tsl::ordered_set<std::string*> set;
  set.insert(value_ptr);
  set.emplace(value_ptr);

  BOOST_CHECK_EQUAL(set.size(), 1);
  BOOST_CHECK_EQUAL(**set.begin(), value);
}

BOOST_AUTO_TEST_CASE(test_erase_if) {
  tsl::ordered_set<int> set{1, 2, 3, 4, 5};
  auto num = erase_if(set, [](int x) { return 2 <= x && x <= 4; });

  BOOST_CHECK_EQUAL(num, 3);
  BOOST_CHECK_EQUAL(set.size(), 2);
  BOOST_CHECK_EQUAL(set.front(), 1);
  BOOST_CHECK_EQUAL(set.back(), 5);
}

/**
 * serialize and deserialize
 */
BOOST_AUTO_TEST_CASE(test_serialize_deserialize) {
  // insert x values; delete some values; serialize set; deserialize in new set;
  // check equal. for deserialization, test it with and without hash
  // compatibility.
  const std::size_t nb_values = 1000;

  tsl::ordered_set<move_only_test> set;
  for (std::size_t i = 0; i < nb_values + 40; i++) {
    set.insert(utils::get_key<move_only_test>(i));
  }

  for (std::size_t i = nb_values; i < nb_values + 40; i++) {
    set.erase(utils::get_key<move_only_test>(i));
  }
  BOOST_CHECK_EQUAL(set.size(), nb_values);

  serializer serial;
  set.serialize(serial);

  deserializer dserial(serial.str());
  auto set_deserialized = decltype(set)::deserialize(dserial, true);
  BOOST_CHECK(set == set_deserialized);

  deserializer dserial2(serial.str());
  set_deserialized = decltype(set)::deserialize(dserial2, false);
  BOOST_CHECK(set_deserialized == set);
}

BOOST_AUTO_TEST_SUITE_END()
