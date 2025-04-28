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
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "tsl/ordered_map.h"
#include "utils.h"

BOOST_AUTO_TEST_SUITE(test_ordered_map)

using test_types = boost::mpl::list<
    tsl::ordered_map<std::int64_t, std::int64_t>,
    tsl::ordered_map<std::int64_t, std::int64_t, std::hash<std::int64_t>,
                     std::equal_to<std::int64_t>,
                     std::allocator<std::pair<std::int64_t, std::int64_t>>,
                     std::vector<std::pair<std::int64_t, std::int64_t>>>,
    tsl::ordered_map<std::string, std::string>,
    tsl::ordered_map<std::string, std::string, mod_hash<9>>,
    tsl::ordered_map<move_only_test, move_only_test, mod_hash<9>>>;

/**
 * insert
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HMap, test_types) {
  // insert x values, insert them again, check values through find, check order
  // through iterator
  using key_tt = typename HMap::key_type;
  using value_tt = typename HMap::mapped_type;

  const std::size_t nb_values = 1000;
  HMap map(0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);

  typename HMap::iterator it;
  bool inserted;

  for (std::size_t i = 0; i < nb_values; i++) {
    // Avoid sequential values by splitting the values with modulo
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_tt>(insert_val),
                    utils::get_value<value_tt>(insert_val)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(insert_val));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(insert_val));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_tt>(insert_val),
                    utils::get_value<value_tt>(insert_val + 1)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(insert_val));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(insert_val));
    BOOST_CHECK(!inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;
    it = map.find(utils::get_key<key_tt>(insert_val));

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(insert_val));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(insert_val));
  }

  std::size_t i = 0;
  for (const auto& key_value : map) {
    const std::size_t insert_val = (i % 2 == 0) ? i : nb_values + i;

    BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_tt>(insert_val));
    BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_tt>(insert_val));

    i++;
  }
}

BOOST_AUTO_TEST_CASE(test_range_insert) {
  // insert x values in vector, range insert x-15 values from vector to map,
  // check values
  const int nb_values = 1000;
  std::vector<std::pair<int, int>> values;
  for (int i = 0; i < nb_values; i++) {
    values.push_back(std::make_pair(i, i + 1));
  }

  tsl::ordered_map<int, int> map = {{-1, 0}, {-2, 0}};
  map.insert(values.begin() + 10, values.end() - 5);

  BOOST_CHECK_EQUAL(map.size(), 987);

  BOOST_CHECK_EQUAL(map.values_container()[0].first, -1);
  BOOST_CHECK_EQUAL(map.values_container()[0].second, 0);
  BOOST_CHECK_EQUAL(map.at(-1), 0);

  BOOST_CHECK_EQUAL(map.values_container()[1].first, -2);
  BOOST_CHECK_EQUAL(map.values_container()[1].second, 0);
  BOOST_CHECK_EQUAL(map.at(-2), 0);

  for (int i = 10, j = 2; i < nb_values - 5; i++, j++) {
    BOOST_CHECK_EQUAL(map.values_container()[j].first, i);
    BOOST_CHECK_EQUAL(map.values_container()[j].second, i + 1);
    BOOST_CHECK_EQUAL(map.at(i), i + 1);
  }
}

BOOST_AUTO_TEST_CASE(test_insert_with_hint) {
  tsl::ordered_map<int, int> map{{1, 0}, {2, 1}, {3, 2}};

  // Wrong hint
  BOOST_CHECK(map.insert(map.find(2), std::make_pair(3, 4)) == map.find(3));

  // Good hint
  BOOST_CHECK(map.insert(map.find(2), std::make_pair(2, 4)) == map.find(2));

  // end() hint
  BOOST_CHECK(map.insert(map.find(10), std::make_pair(2, 4)) == map.find(2));

  BOOST_CHECK_EQUAL(map.size(), 3);

  // end() hint, new value
  BOOST_CHECK_EQUAL(map.insert(map.find(10), std::make_pair(4, 3))->first, 4);

  // Wrong hint, new value
  BOOST_CHECK_EQUAL(map.insert(map.find(2), std::make_pair(5, 4))->first, 5);

  BOOST_CHECK_EQUAL(map.size(), 5);
}

BOOST_AUTO_TEST_CASE(test_insert_copy_constructible_only_values) {
  // Test that an ordered_map with copy constructible only objects compiles
  const std::size_t nb_values = 100;

  tsl::ordered_map<copy_constructible_only_test, copy_constructible_only_test>
      map;
  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {copy_constructible_only_test(i), copy_constructible_only_test(i * 2)});
  }

  BOOST_CHECK_EQUAL(map.size(), nb_values);
}

/**
 * emplace_hint
 */
BOOST_AUTO_TEST_CASE(test_emplace_hint) {
  tsl::ordered_map<int, int> map{{1, 0}, {2, 1}, {3, 2}};

  // Wrong hint
  BOOST_CHECK(map.emplace_hint(map.find(2), std::piecewise_construct,
                               std::forward_as_tuple(3),
                               std::forward_as_tuple(4)) == map.find(3));

  // Good hint
  BOOST_CHECK(map.emplace_hint(map.find(2), std::piecewise_construct,
                               std::forward_as_tuple(2),
                               std::forward_as_tuple(4)) == map.find(2));

  // end() hint
  BOOST_CHECK(map.emplace_hint(map.find(10), std::piecewise_construct,
                               std::forward_as_tuple(2),
                               std::forward_as_tuple(4)) == map.find(2));

  BOOST_CHECK_EQUAL(map.size(), 3);

  // end() hint, new value
  BOOST_CHECK_EQUAL(
      map.emplace_hint(map.find(10), std::piecewise_construct,
                       std::forward_as_tuple(4), std::forward_as_tuple(3))
          ->first,
      4);

  // Wrong hint, new value
  BOOST_CHECK_EQUAL(
      map.emplace_hint(map.find(2), std::piecewise_construct,
                       std::forward_as_tuple(5), std::forward_as_tuple(4))
          ->first,
      5);

  BOOST_CHECK_EQUAL(map.size(), 5);
}

/**
 * emplace
 */
BOOST_AUTO_TEST_CASE(test_emplace) {
  tsl::ordered_map<std::int64_t, move_only_test> map;
  tsl::ordered_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(10),
                  std::forward_as_tuple(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(10),
                  std::forward_as_tuple(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(!inserted);
}

/**
 * try_emplace
 */
BOOST_AUTO_TEST_CASE(test_try_emplace) {
  tsl::ordered_map<std::int64_t, move_only_test> map;
  tsl::ordered_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) = map.try_emplace(10, 1);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) = map.try_emplace(10, 3);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(!inserted);
}

BOOST_AUTO_TEST_CASE(test_try_emplace_2) {
  tsl::ordered_map<std::string, move_only_test> map;
  tsl::ordered_map<std::string, move_only_test>::iterator it;
  bool inserted;

  const std::size_t nb_values = 1000;
  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) = map.try_emplace(utils::get_key<std::string>(i), i);

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) =
        map.try_emplace(utils::get_key<std::string>(i), i + 1);

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
    BOOST_CHECK(!inserted);
  }

  for (std::size_t i = 0; i < nb_values; i++) {
    it = map.find(utils::get_key<std::string>(i));

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
  }
}

BOOST_AUTO_TEST_CASE(test_try_emplace_hint) {
  tsl::ordered_map<std::int64_t, move_only_test> map(0);

  // end() hint, new value
  auto it = map.try_emplace(map.find(10), 10, 1);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Good hint
  it = map.try_emplace(map.find(10), 10, 3);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Wrong hint, new value
  it = map.try_emplace(map.find(10), 1, 3);
  BOOST_CHECK_EQUAL(it->first, 1);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
}

/**
 * insert_or_assign
 */
BOOST_AUTO_TEST_CASE(test_insert_or_assign) {
  tsl::ordered_map<std::int64_t, move_only_test> map;
  tsl::ordered_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
  BOOST_CHECK(!inserted);
}

BOOST_AUTO_TEST_CASE(test_insert_or_assign_hint) {
  tsl::ordered_map<std::int64_t, move_only_test> map(0);

  // end() hint, new value
  auto it = map.insert_or_assign(map.find(10), 10, move_only_test(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Good hint
  it = map.insert_or_assign(map.find(10), 10, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));

  // Bad hint, new value
  it = map.insert_or_assign(map.find(10), 1, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 1);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
}

/**
 * insert_at_position
 */
BOOST_AUTO_TEST_CASE(test_insert_at_position) {
  tsl::ordered_map<std::string, int> map = {
      {"Key2", 2}, {"Key4", 4},   {"Key6", 6},   {"Key8", 8},
      {"Key9", 9}, {"Key10", 10}, {"Key11", 11}, {"Key12", 12}};

  BOOST_CHECK(map.insert_at_position(map.begin(), {"Key1", 1}).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12}}));

  auto it = map.insert_at_position(map.nth(2), {"Key3", 3});
  BOOST_CHECK(*it.first == (std::pair<std::string, int>("Key3", 3)));
  BOOST_CHECK(it.second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12}}));

  BOOST_CHECK(map.insert_at_position(map.end(), {"Key7", 7}).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));

  BOOST_CHECK(map.insert_at_position(map.nth(4), {"Key5", 5}).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key5", 5},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));

  it = map.insert_at_position(map.nth(3), {"Key8", 8});
  BOOST_CHECK(*it.first == (std::pair<std::string, int>("Key8", 8)));
  BOOST_CHECK(!it.second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key5", 5},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));
}

BOOST_AUTO_TEST_CASE(test_insert_at_position_high_collisions) {
  tsl::ordered_map<int, int, identity_hash<int>> map(32);
  BOOST_CHECK_EQUAL(map.bucket_count(), 32);
  map.insert({{0, 0}, {32, -32}, {64, -64}, {96, -96}, {128, -128}});

  auto it = map.insert_at_position(map.begin(), {160, -160});
  BOOST_CHECK(*it.first == (std::pair<int, int>(160, -160)));
  BOOST_CHECK(it.second);
  BOOST_CHECK(utils::test_is_equal(
      map,
      tsl::ordered_map<int, int, identity_hash<int>>{
          {160, -160}, {0, 0}, {32, -32}, {64, -64}, {96, -96}, {128, -128}}));
}

/**
 * try_emplace_at_position
 */
BOOST_AUTO_TEST_CASE(test_try_emplace_at_position) {
  tsl::ordered_map<std::string, int> map = {
      {"Key2", 2}, {"Key4", 4},   {"Key6", 6},   {"Key8", 8},
      {"Key9", 9}, {"Key10", 10}, {"Key11", 11}, {"Key12", 12}};

  BOOST_CHECK(map.try_emplace_at_position(map.begin(), "Key1", 1).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12}}));

  auto it = map.try_emplace_at_position(map.nth(2), "Key3", 3);
  BOOST_CHECK(*it.first == (std::pair<std::string, int>("Key3", 3)));
  BOOST_CHECK(it.second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12}}));

  BOOST_CHECK(map.try_emplace_at_position(map.end(), "Key7", 7).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));

  BOOST_CHECK(map.try_emplace_at_position(map.nth(4), "Key5", 5).second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key5", 5},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));

  it = map.try_emplace_at_position(map.nth(3), "Key8", 8);
  BOOST_CHECK(*it.first == (std::pair<std::string, int>("Key8", 8)));
  BOOST_CHECK(!it.second);
  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"Key1", 1},
                                              {"Key2", 2},
                                              {"Key3", 3},
                                              {"Key4", 4},
                                              {"Key5", 5},
                                              {"Key6", 6},
                                              {"Key8", 8},
                                              {"Key9", 9},
                                              {"Key10", 10},
                                              {"Key11", 11},
                                              {"Key12", 12},
                                              {"Key7", 7}}));
}

/**
 * erase
 */
BOOST_AUTO_TEST_CASE(test_range_erase_all) {
  // insert x values, delete all
  using HMap = tsl::ordered_map<std::string, std::int64_t>;

  const std::size_t nb_values = 1000;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.erase(map.begin(), map.end());
  BOOST_CHECK(it == map.end());
  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE(test_range_erase) {
  // insert x values, delete all with iterators except 10 first and 780 last
  // values
  using HMap = tsl::ordered_map<std::string, std::int64_t>;

  const std::size_t nb_values = 1000;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it_first = std::next(map.begin(), 10);
  auto it_last = std::next(map.begin(), 220);

  auto it = map.erase(it_first, it_last);
  BOOST_CHECK_EQUAL(std::distance(it, map.end()), 780);
  BOOST_CHECK_EQUAL(map.size(), 790);
  BOOST_CHECK_EQUAL(std::distance(map.begin(), map.end()), 790);

  for (auto& val : map) {
    BOOST_CHECK_EQUAL(map.count(val.first), 1);
  }

  // Check order
  it = map.begin();
  for (std::size_t i = 0; i < nb_values; i++) {
    if (i >= 10 && i < 220) {
      continue;
    }
    BOOST_CHECK(*it == (std::pair<std::string, std::int64_t>(
                           utils::get_key<std::string>(i),
                           utils::get_value<std::int64_t>(i))));
    ++it;
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_loop, HMap, test_types) {
  // insert x values, delete all one by one
  std::size_t nb_values = 1000;

  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map2 = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.begin();
  // Use second map to check for key after delete as we may not copy the key
  // with move-only types.
  auto it2 = map2.begin();
  while (it != map.end()) {
    it = map.erase(it);
    --nb_values;

    BOOST_CHECK_EQUAL(map.count(it2->first), 0);
    BOOST_CHECK_EQUAL(map.size(), nb_values);
    ++it2;
  }

  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_loop_range, HMap, test_types) {
  // insert x values, delete all five by five
  const std::size_t hop = 5;
  std::size_t nb_values = 1000;

  BOOST_REQUIRE_EQUAL(nb_values % hop, 0);

  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.begin();
  while (it != map.end()) {
    it = map.erase(it, std::next(it, hop));
    nb_values -= hop;

    BOOST_CHECK_EQUAL(map.size(), nb_values);
  }

  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert_erase_insert, HMap, test_types) {
  // insert x/2 values, delete x/4 values, insert x/2 values, find each value,
  // check order of values
  using key_tt = typename HMap::key_type;
  using value_tt = typename HMap::mapped_type;

  const std::size_t nb_values = 2000;
  HMap map(10);
  typename HMap::iterator it;
  bool inserted;

  // Insert nb_values/2
  for (std::size_t i = 0; i < nb_values / 2; i++) {
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_tt>(i), utils::get_value<value_tt>(i)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values / 2);

  // Delete nb_values/4
  for (std::size_t i = 0; i < nb_values / 2; i++) {
    if (i % 2 == 0) {
      BOOST_CHECK_EQUAL(map.erase(utils::get_key<key_tt>(i)), 1);
    }
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values / 4);

  // Insert nb_values/2
  for (std::size_t i = nb_values / 2; i < nb_values; i++) {
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_tt>(i), utils::get_value<value_tt>(i)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values - nb_values / 4);

  // Find
  for (std::size_t i = 0; i < nb_values; i++) {
    if (i % 2 == 0 && i < nb_values / 2) {
      it = map.find(utils::get_key<key_tt>(i));

      BOOST_CHECK(it == map.cend());
    } else {
      it = map.find(utils::get_key<key_tt>(i));

      BOOST_REQUIRE(it != map.end());
      BOOST_CHECK_EQUAL(it->first, utils::get_key<key_tt>(i));
      BOOST_CHECK_EQUAL(it->second, utils::get_value<value_tt>(i));
    }
  }

  // Check order
  std::size_t i = 1;
  for (const auto& key_value : map) {
    if (i < nb_values / 2) {
      BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_tt>(i));
      BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_tt>(i));

      i = std::min(i + 2, nb_values / 2);
    } else {
      BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_tt>(i));
      BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_tt>(i));

      i++;
    }
  }
}

BOOST_AUTO_TEST_CASE(test_range_erase_same_iterators) {
  // insert x values, test erase with same iterator as each parameter, check if
  // returned mutable iterator is valid.
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::ordered_map<std::int64_t, std::int64_t>>(
          nb_values);

  tsl::ordered_map<std::int64_t, std::int64_t>::const_iterator it_const =
      map.cbegin();
  std::advance(it_const, 10);

  tsl::ordered_map<std::int64_t, std::int64_t>::iterator it_mutable =
      map.erase(it_const, it_const);
  BOOST_CHECK(it_const == it_mutable);
  BOOST_CHECK(map.mutable_iterator(it_const) == it_mutable);
  BOOST_CHECK_EQUAL(map.size(), 100);

  it_mutable.value() = -100;
  BOOST_CHECK_EQUAL(it_const.value(), -100);
}

/**
 * erase_if
 */
BOOST_AUTO_TEST_CASE(test_erase_if) {
  using map_type = tsl::ordered_map<int, int>;
  using value_type = map_type::value_type;
  tsl::ordered_map<int, int> map{{0, 2}, {16, 2}, {24, 2}, {5, 5},
                                 {6, 2}, {7, 7},  {8, 8},  {9, 9}};

  auto n = erase_if(map, [](const value_type& x) { return x.second == 2; });
  BOOST_CHECK_EQUAL(n, 4);

  n = erase_if(map, [](const value_type& x) { return x.second == 2; });
  BOOST_CHECK_EQUAL(n, 0);

  BOOST_CHECK_EQUAL(map.size(), 4);

  BOOST_CHECK_EQUAL(map.at(5), 5);
  BOOST_CHECK_EQUAL(map.at(7), 7);
  BOOST_CHECK_EQUAL(map.at(8), 8);
  BOOST_CHECK_EQUAL(map.at(9), 9);

  BOOST_CHECK(map.find(0) == map.end());
  BOOST_CHECK(map.find(6) == map.end());
  BOOST_CHECK(map.find(16) == map.end());
  BOOST_CHECK(map.find(24) == map.end());
}

/**
 * unordered_erase
 */
BOOST_AUTO_TEST_CASE(test_unordered_erase) {
  std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::ordered_map<std::int64_t, std::int64_t>>(
          nb_values);

  BOOST_CHECK_EQUAL(map.unordered_erase(3), 1);
  BOOST_CHECK_EQUAL(map.size(), --nb_values);

  BOOST_CHECK_EQUAL(map.unordered_erase(-1), 0);
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  auto it = map.begin();
  while (it != map.end()) {
    it = map.unordered_erase(it);
    BOOST_CHECK_EQUAL(map.size(), --nb_values);
  }

  BOOST_CHECK_EQUAL(map.size(), 0);
}

/**
 * rehash
 */
BOOST_AUTO_TEST_CASE(test_rehash_empty) {
  // test rehash(0), test find/erase/insert on map.
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::ordered_map<std::int64_t, std::int64_t>>(
          nb_values);

  const std::size_t bucket_count = map.bucket_count();
  BOOST_CHECK(bucket_count >= nb_values);

  map.clear();
  BOOST_CHECK_EQUAL(map.bucket_count(), bucket_count);
  BOOST_CHECK(map.empty());

  map.rehash(0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK(map.empty());

  BOOST_CHECK(map.find(1) == map.end());
  BOOST_CHECK_EQUAL(map.erase(1), 0);
  BOOST_CHECK(map.insert({1, 10}).second);
  BOOST_CHECK_EQUAL(map.at(1), 10);
}

/**
 * operator== and operator!=
 */
BOOST_AUTO_TEST_CASE(test_compare) {
  const tsl::ordered_map<std::string, int> map = {{"D", 1}, {"L", 2}, {"A", 3}};

  BOOST_ASSERT(utils::test_is_equal(
      map, tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"A", 3}}));
  BOOST_ASSERT(map != (tsl::ordered_map<std::string, int>{
                          {"L", 2}, {"D", 1}, {"A", 3}}));

  BOOST_ASSERT(
      map < (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"B", 3}}));
  BOOST_ASSERT(map <= (tsl::ordered_map<std::string, int>{
                          {"D", 1}, {"L", 2}, {"B", 3}}));
  BOOST_ASSERT(map <= (tsl::ordered_map<std::string, int>{
                          {"D", 1}, {"L", 2}, {"A", 3}}));

  BOOST_ASSERT(
      map > (tsl::ordered_map<std::string, int>{{"D", 1}, {"K", 2}, {"A", 3}}));
  BOOST_ASSERT(map >= (tsl::ordered_map<std::string, int>{
                          {"D", 1}, {"K", 2}, {"A", 3}}));
  BOOST_ASSERT(map >= (tsl::ordered_map<std::string, int>{
                          {"D", 1}, {"L", 2}, {"A", 3}}));
}

/**
 * clear
 */
BOOST_AUTO_TEST_CASE(test_clear) {
  // insert x values, clear map, insert new values
  using HMap = tsl::ordered_map<std::int64_t, std::int64_t>;

  const std::size_t nb_values = 1000;
  auto map = utils::get_filled_hash_map<HMap>(nb_values);

  map.clear();
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(std::distance(map.begin(), map.end()), 0);

  map.insert({5, -5});
  map.insert({{1, -1}, {2, -1}, {4, -4}, {3, -3}});

  BOOST_CHECK(utils::test_is_equal(
      map, HMap({{5, -5}, {1, -1}, {2, -1}, {4, -4}, {3, -3}})));
}

/**
 * iterator
 */
BOOST_AUTO_TEST_CASE(test_reverse_iterator) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{1, 1}, {-2, 2}, {3, 3}};
  map[2] = 4;

  std::size_t i = 4;
  for (auto it = map.rbegin(); it != map.rend(); ++it) {
    BOOST_CHECK_EQUAL(it->second, i);
    i--;
  }

  i = 4;
  for (auto it = map.rcbegin(); it != map.rcend(); ++it) {
    BOOST_CHECK_EQUAL(it->second, i);
    i--;
  }
}

BOOST_AUTO_TEST_CASE(test_iterator_arithmetic) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {
      {1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}};

  tsl::ordered_map<std::int64_t, std::int64_t>::const_iterator it;
  tsl::ordered_map<std::int64_t, std::int64_t>::const_iterator it2;

  it = map.cbegin();
  // it += n
  it += 3;
  BOOST_CHECK_EQUAL(it->second, 40);

  // it + n
  BOOST_CHECK_EQUAL((map.cbegin() + 3)->second, 40);
  // n + it
  BOOST_CHECK_EQUAL((3 + map.cbegin())->second, 40);

  it = map.cbegin() + 4;
  // it -= n
  it -= 2;
  BOOST_CHECK_EQUAL(it->second, 30);

  // it - n
  BOOST_CHECK_EQUAL((it - 1)->second, 20);

  it = map.cbegin() + 2;
  it2 = map.cbegin() + 4;
  // it - it
  BOOST_CHECK_EQUAL(it2 - it, 2);

  // it[n]
  BOOST_CHECK_EQUAL(map.cbegin()[2].second, 30);

  it = map.cbegin() + 1;
  // it[n]
  BOOST_CHECK_EQUAL(it[2].second, 40);

  it = map.cbegin();
  // it++
  it++;
  BOOST_CHECK_EQUAL((it++)->second, 20);

  it = map.cbegin();
  // ++it
  ++it;
  BOOST_CHECK_EQUAL((++it)->second, 30);

  it = map.cend();
  // it--
  it--;
  BOOST_CHECK_EQUAL((it--)->second, 60);

  it = map.cend();
  // --it
  --it;
  BOOST_CHECK_EQUAL((--it)->second, 50);
}

BOOST_AUTO_TEST_CASE(test_iterator_compartors) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {
      {1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}};

  tsl::ordered_map<std::int64_t, std::int64_t>::const_iterator it;
  tsl::ordered_map<std::int64_t, std::int64_t>::const_iterator it2;

  it = map.begin() + 1;
  it2 = map.end() - 1;

  BOOST_CHECK(it < it2);
  BOOST_CHECK(it <= it2);
  BOOST_CHECK(it2 > it);
  BOOST_CHECK(it2 >= it);

  it = map.begin() + 3;
  it2 = map.end() - 3;

  BOOST_CHECK(it == it2);
  BOOST_CHECK(it <= it2);
  BOOST_CHECK(it >= it2);
  BOOST_CHECK(it2 <= it);
  BOOST_CHECK(it2 >= it);
  BOOST_CHECK(!(it < it2));
  BOOST_CHECK(!(it > it2));
  BOOST_CHECK(!(it2 < it));
  BOOST_CHECK(!(it2 > it));
}

/**
 * iterator.value()
 */
BOOST_AUTO_TEST_CASE(test_modify_value) {
  // insert x values, modify value of even keys, check values
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::ordered_map<std::int64_t, std::int64_t>>(
          nb_values);

  for (auto it = map.begin(); it != map.end(); ++it) {
    if (it->first % 2 == 0) {
      it.value() = -1;
    }
  }

  for (auto& val : map) {
    if (val.first % 2 == 0) {
      BOOST_CHECK_EQUAL(val.second, -1);
    } else {
      BOOST_CHECK_NE(val.second, -1);
    }
  }
}

/**
 * max_size
 */
BOOST_AUTO_TEST_CASE(test_max_size) {
  // TODO not compatible on systems with sizeof(std::size_t) <
  // sizeof(std::uint32_t), will not build.
  tsl::ordered_map<int, int, std::hash<int>, std::equal_to<int>,
                   std::allocator<std::pair<int, int>>,
                   std::vector<std::pair<int, int>>, std::uint16_t>
      map;
  BOOST_CHECK(map.max_size() <= std::numeric_limits<std::uint16_t>::max());
  BOOST_CHECK(map.max_size() > std::numeric_limits<std::uint8_t>::max());

  tsl::ordered_map<int, int, std::hash<int>, std::equal_to<int>,
                   std::allocator<std::pair<int, int>>,
                   std::vector<std::pair<int, int>>, std::uint32_t>
      map2;
  BOOST_CHECK(map2.max_size() <= std::numeric_limits<std::uint32_t>::max());
  BOOST_CHECK(map2.max_size() > std::numeric_limits<std::uint16_t>::max());

  using max_size_type =
      std::conditional<sizeof(std::size_t) == sizeof(std::uint64_t),
                       std::uint64_t, std::uint32_t>::type;
  using min_size_type =
      std::conditional<sizeof(std::size_t) == sizeof(std::uint64_t),
                       std::uint32_t, std::uint16_t>::type;
  tsl::ordered_map<int, int, std::hash<int>, std::equal_to<int>,
                   std::allocator<std::pair<int, int>>,
                   std::vector<std::pair<int, int>>, max_size_type>
      map3;
  BOOST_CHECK(map3.max_size() <= std::numeric_limits<max_size_type>::max());
  BOOST_CHECK(map3.max_size() > std::numeric_limits<min_size_type>::max());
}

/**
 * constructor
 */
BOOST_AUTO_TEST_CASE(test_extreme_bucket_count_value_construction) {
  TSL_OH_CHECK_THROW(
      (tsl::ordered_map<int, int>(std::numeric_limits<std::size_t>::max())),
      std::length_error);
}

BOOST_AUTO_TEST_CASE(test_range_construct) {
  tsl::ordered_map<int, int> map = {{2, 1}, {1, 0}, {3, 2}};

  tsl::ordered_map<int, int> map2(map.begin(), map.end());
  tsl::ordered_map<int, int> map3(map.cbegin(), map.cend());
}

/**
 * operator=(std::initializer_list)
 */
BOOST_AUTO_TEST_CASE(test_assign_operator) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};
  BOOST_CHECK_EQUAL(map.size(), 2);

  map = {{1, 3}, {2, 4}};
  BOOST_CHECK_EQUAL(map.size(), 2);
  BOOST_CHECK_EQUAL(map.at(1), 3);
  BOOST_CHECK_EQUAL(map.at(2), 4);
  BOOST_CHECK(map.find(0) == map.end());

  map = {};
  BOOST_CHECK(map.empty());
}

/**
 * move/copy constructor/operator
 */
BOOST_AUTO_TEST_CASE(test_move_constructor) {
  // insert x values in map, move map into map_move, check map and map_move,
  // insert additional values in map_move, check map_move
  using HMap = tsl::ordered_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(std::move(map));

  BOOST_CHECK(utils::test_is_equal(
      map_move, utils::get_filled_hash_map<HMap>(nb_values)));
  BOOST_CHECK(utils::test_is_equal(map, HMap()));

  for (std::size_t i = nb_values; i < nb_values * 2; i++) {
    map_move.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map_move.size(), nb_values * 2);
  BOOST_CHECK(utils::test_is_equal(
      map_move, utils::get_filled_hash_map<HMap>(nb_values * 2)));
}

BOOST_AUTO_TEST_CASE(test_move_constructor_empty) {
  tsl::ordered_map<std::string, move_only_test> map(0);
  tsl::ordered_map<std::string, move_only_test> map_move(std::move(map));

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_move.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_move.find("") == map_move.end());
}

BOOST_AUTO_TEST_CASE(test_move_operator) {
  // insert x values in map, move map into map_move, check map and map_move,
  // insert additional values in map_move, check map_move
  using HMap = tsl::ordered_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move = utils::get_filled_hash_map<HMap>(1);
  map_move = std::move(map);

  BOOST_CHECK(utils::test_is_equal(
      map_move, utils::get_filled_hash_map<HMap>(nb_values)));
  BOOST_CHECK(utils::test_is_equal(map, HMap()));

  for (std::size_t i = nb_values; i < nb_values * 2; i++) {
    map_move.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map_move.size(), nb_values * 2);
  BOOST_CHECK(utils::test_is_equal(
      map_move, utils::get_filled_hash_map<HMap>(nb_values * 2)));
}

BOOST_AUTO_TEST_CASE(test_move_operator_empty) {
  tsl::ordered_map<std::string, move_only_test> map(0);
  tsl::ordered_map<std::string, move_only_test> map_move;
  map_move = (std::move(map));

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_move.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_move.find("") == map_move.end());
}

BOOST_AUTO_TEST_CASE(test_reassign_moved_object_move_constructor) {
  using HMap = tsl::ordered_map<std::string, std::string>;

  HMap map = {{"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  HMap map_move(std::move(map));

  BOOST_CHECK_EQUAL(map_move.size(), 3);
  BOOST_CHECK_EQUAL(map.size(), 0);

  map = {{"Key4", "Value4"}, {"Key5", "Value5"}};
  BOOST_CHECK(utils::test_is_equal(
      map, HMap({{"Key4", "Value4"}, {"Key5", "Value5"}})));
}

BOOST_AUTO_TEST_CASE(test_reassign_moved_object_move_operator) {
  using HMap = tsl::ordered_map<std::string, std::string>;

  HMap map = {{"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  HMap map_move = std::move(map);

  BOOST_CHECK_EQUAL(map_move.size(), 3);
  BOOST_CHECK_EQUAL(map.size(), 0);

  map = {{"Key4", "Value4"}, {"Key5", "Value5"}};
  BOOST_CHECK(utils::test_is_equal(
      map, HMap({{"Key4", "Value4"}, {"Key5", "Value5"}})));
}

BOOST_AUTO_TEST_CASE(test_use_after_move_constructor) {
  using HMap = tsl::ordered_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(std::move(map));

  BOOST_CHECK(utils::test_is_equal(map, HMap()));
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.erase("a"), 0);
  BOOST_CHECK(map.find("a") == map.end());

  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map.size(), nb_values);
  BOOST_CHECK(utils::test_is_equal(map, map_move));
}

BOOST_AUTO_TEST_CASE(test_use_after_move_operator) {
  using HMap = tsl::ordered_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(0);
  map_move = std::move(map);

  BOOST_CHECK(utils::test_is_equal(map, HMap()));
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.erase("a"), 0);
  BOOST_CHECK(map.find("a") == map.end());

  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map.size(), nb_values);
  BOOST_CHECK(utils::test_is_equal(map, map_move));
}

BOOST_AUTO_TEST_CASE(test_copy_constructor_and_operator) {
  using HMap = tsl::ordered_map<std::string, std::string, mod_hash<9>>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  HMap map_copy = map;
  HMap map_copy2(map);
  HMap map_copy3 = utils::get_filled_hash_map<HMap>(1);
  map_copy3 = map;

  BOOST_CHECK(utils::test_is_equal(map, map_copy));
  map.clear();

  BOOST_CHECK(utils::test_is_equal(map_copy, map_copy2));
  BOOST_CHECK(utils::test_is_equal(map_copy, map_copy3));
}

BOOST_AUTO_TEST_CASE(test_copy_constructor_empty) {
  tsl::ordered_map<std::string, int> map(0);
  tsl::ordered_map<std::string, int> map_copy(map);

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_copy.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_copy.find("") == map_copy.end());
}

BOOST_AUTO_TEST_CASE(test_copy_operator_empty) {
  tsl::ordered_map<std::string, int> map(0);
  tsl::ordered_map<std::string, int> map_copy(16);
  map_copy = map;

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_copy.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_copy.find("") == map_copy.end());
}

/**
 * at
 */
BOOST_AUTO_TEST_CASE(test_at) {
  // insert x values, use at for known and unknown values.
  const tsl::ordered_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK_EQUAL(map.at(0), 10);
  BOOST_CHECK_EQUAL(map.at(-2), 20);
  TSL_OH_CHECK_THROW(map.at(1), std::out_of_range);
}

/**
 * contains
 */
BOOST_AUTO_TEST_CASE(test_contains) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK(map.contains(0));
  BOOST_CHECK(map.contains(-2));
  BOOST_CHECK(!map.contains(-3));
}

/**
 * equal_range
 */
BOOST_AUTO_TEST_CASE(test_equal_range) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  auto it_pair = map.equal_range(0);
  BOOST_REQUIRE_EQUAL(std::distance(it_pair.first, it_pair.second), 1);
  BOOST_CHECK_EQUAL(it_pair.first->second, 10);

  it_pair = map.equal_range(1);
  BOOST_CHECK(it_pair.first == it_pair.second);
  BOOST_CHECK(it_pair.first == map.end());
}

/**
 * release
 */
BOOST_AUTO_TEST_CASE(test_release) {
  auto vec = std::deque<std::pair<int, int>>{{1, 1}, {2, 2}, {3, 3}};
  auto map = tsl::ordered_map<int, int>{vec.begin(), vec.end()};

  BOOST_CHECK(map.release() == vec);
  BOOST_CHECK(map.empty());
}

/**
 * data()
 */
BOOST_AUTO_TEST_CASE(test_data) {
  tsl::ordered_map<std::int64_t, std::int64_t, std::hash<std::int64_t>,
                   std::equal_to<std::int64_t>,
                   std::allocator<std::pair<std::int64_t, std::int64_t>>,
                   std::vector<std::pair<std::int64_t, std::int64_t>>>
      map = {{1, -1}, {2, -2}, {4, -4}, {3, -3}};

  BOOST_CHECK(map.data() == map.values_container().data());
}

/**
 * operator[]
 */
BOOST_AUTO_TEST_CASE(test_access_operator) {
  // insert x values, use at for known and unknown values.
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK_EQUAL(map[0], 10);
  BOOST_CHECK_EQUAL(map[-2], 20);
  BOOST_CHECK_EQUAL(map[2], std::int64_t());

  BOOST_CHECK_EQUAL(map.size(), 3);
}

/**
 * swap
 */
BOOST_AUTO_TEST_CASE(test_swap) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {
      {1, 10}, {8, 80}, {3, 30}};
  tsl::ordered_map<std::int64_t, std::int64_t> map2 = {{4, 40}, {5, 50}};

  using std::swap;
  swap(map, map2);

  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::int64_t, std::int64_t>{{4, 40}, {5, 50}}));
  BOOST_CHECK(utils::test_is_equal(
      map2,
      tsl::ordered_map<std::int64_t, std::int64_t>{{1, 10}, {8, 80}, {3, 30}}));

  map.insert({6, 60});
  map2.insert({4, 40});

  BOOST_CHECK(utils::test_is_equal(
      map,
      tsl::ordered_map<std::int64_t, std::int64_t>{{4, 40}, {5, 50}, {6, 60}}));
  BOOST_CHECK(
      utils::test_is_equal(map2, tsl::ordered_map<std::int64_t, std::int64_t>{
                                     {1, 10}, {8, 80}, {3, 30}, {4, 40}}));
}

BOOST_AUTO_TEST_CASE(test_swap_empty) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {
      {1, 10}, {8, 80}, {3, 30}};
  tsl::ordered_map<std::int64_t, std::int64_t> map2;

  using std::swap;
  swap(map, map2);

  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::int64_t, std::int64_t>{}));
  BOOST_CHECK(utils::test_is_equal(
      map2,
      tsl::ordered_map<std::int64_t, std::int64_t>{{1, 10}, {8, 80}, {3, 30}}));

  map.insert({6, 60});
  map2.insert({4, 40});

  BOOST_CHECK(utils::test_is_equal(
      map, tsl::ordered_map<std::int64_t, std::int64_t>{{6, 60}}));
  BOOST_CHECK(
      utils::test_is_equal(map2, tsl::ordered_map<std::int64_t, std::int64_t>{
                                     {1, 10}, {8, 80}, {3, 30}, {4, 40}}));
}

/**
 * serialize and deserialize
 */
BOOST_AUTO_TEST_CASE(test_serialize_deserialize_empty) {
  // serialize empty map; deserialize in new map; check equal.
  // for deserialization, test it with and without hash compatibility.
  const tsl::ordered_map<std::string, move_only_test> empty_map(0);

  serializer serial;
  empty_map.serialize(serial);

  deserializer dserial(serial.str());
  auto empty_map_deserialized = decltype(empty_map)::deserialize(dserial, true);
  BOOST_CHECK(utils::test_is_equal(empty_map_deserialized, empty_map));

  deserializer dserial2(serial.str());
  empty_map_deserialized = decltype(empty_map)::deserialize(dserial2, false);
  BOOST_CHECK(utils::test_is_equal(empty_map_deserialized, empty_map));
}

BOOST_AUTO_TEST_CASE(test_serialize_deserialize) {
  // insert x values; delete some values; serialize map; deserialize in new map;
  // check equal. for deserialization, test it with and without hash
  // compatibility.
  const std::size_t nb_values = 1000;

  tsl::ordered_map<std::string, move_only_test> map;
  for (std::size_t i = 0; i < nb_values + 40; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  for (std::size_t i = nb_values; i < nb_values + 40; i++) {
    map.erase(utils::get_key<std::string>(i));
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  serializer serial;
  map.serialize(serial);

  deserializer dserial(serial.str());
  auto map_deserialized = decltype(map)::deserialize(dserial, true);
  BOOST_CHECK(utils::test_is_equal(map, map_deserialized));

  deserializer dserial2(serial.str());
  map_deserialized = decltype(map)::deserialize(dserial2, false);
  BOOST_CHECK(utils::test_is_equal(map_deserialized, map));
}

BOOST_AUTO_TEST_CASE(test_serialize_deserialize_with_different_hash) {
  // insert x values; serialize map; deserialize in new map which has a
  // different hash; check equal
  struct hash_str_diff {
    std::size_t operator()(const std::string& str) const {
      return std::hash<std::string>()(str) + 123;
    }
  };

  const std::size_t nb_values = 1000;

  tsl::ordered_map<std::string, move_only_test> map;
  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  serializer serial;
  map.serialize(serial);

  deserializer dserial(serial.str());
  auto map_deserialized =
      tsl::ordered_map<std::string, move_only_test, hash_str_diff>::deserialize(
          dserial, false);

  BOOST_CHECK_EQUAL(map_deserialized.size(), map.size());
  for (const auto& val : map) {
    BOOST_CHECK(map_deserialized.find(val.first) != map_deserialized.end());
  }
}

/**
 * front(), back()
 */
BOOST_AUTO_TEST_CASE(test_front_back) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{1, 10}, {2, 20}};
  map.insert({0, 0});

  BOOST_CHECK(map.front() == (std::pair<std::int64_t, std::int64_t>(1, 10)));
  BOOST_CHECK(map.back() == (std::pair<std::int64_t, std::int64_t>(0, 0)));

  map.clear();
  map.insert({3, 30});
  BOOST_CHECK(map.front() == (std::pair<std::int64_t, std::int64_t>(3, 30)));
  BOOST_CHECK(map.back() == (std::pair<std::int64_t, std::int64_t>(3, 30)));
}

/**
 * nth()
 */
BOOST_AUTO_TEST_CASE(test_nth) {
  tsl::ordered_map<std::int64_t, std::int64_t> map = {{1, 10}, {2, 20}};
  map.insert({0, 0});

  BOOST_REQUIRE(map.nth(0) != map.end());
  BOOST_CHECK(*map.nth(0) == (std::pair<std::int64_t, std::int64_t>(1, 10)));

  BOOST_REQUIRE(map.nth(1) != map.end());
  BOOST_CHECK(*map.nth(1) == (std::pair<std::int64_t, std::int64_t>(2, 20)));

  BOOST_REQUIRE(map.nth(2) != map.end());
  BOOST_CHECK(*map.nth(2) == (std::pair<std::int64_t, std::int64_t>(0, 0)));

  BOOST_REQUIRE(map.nth(3) == map.end());

  map.clear();
  BOOST_REQUIRE(map.nth(0) == map.end());
}

/**
 * KeyEqual
 */
BOOST_AUTO_TEST_CASE(test_key_equal) {
  // Use a KeyEqual and Hash where any odd unsigned number 'x' is equal to
  // 'x-1'. Make sure that KeyEqual is called (and not ==).
  struct hash {
    std::size_t operator()(std::uint64_t v) const {
      if (v % 2u == 1u) {
        return std::hash<std::uint64_t>()(v - 1);
      } else {
        return std::hash<std::uint64_t>()(v);
      }
    }
  };

  struct key_equal {
    bool operator()(std::uint64_t lhs, std::uint64_t rhs) const {
      if (lhs % 2u == 1u) {
        lhs--;
      }

      if (rhs % 2u == 1u) {
        rhs--;
      }

      return lhs == rhs;
    }
  };

  tsl::ordered_map<std::uint64_t, std::uint64_t, hash, key_equal> map;
  BOOST_CHECK(map.insert({2, 10}).second);
  BOOST_CHECK_EQUAL(map.at(2), 10);
  BOOST_CHECK_EQUAL(map.at(3), 10);
  BOOST_CHECK(!map.insert({3, 10}).second);

  BOOST_CHECK_EQUAL(map.size(), 1);
}

/**
 * other
 */
BOOST_AUTO_TEST_CASE(test_heterogeneous_lookups) {
  struct hash_ptr {
    std::size_t operator()(const std::unique_ptr<int>& p) const {
      return std::hash<std::uintptr_t>()(
          reinterpret_cast<std::uintptr_t>(p.get()));
    }

    std::size_t operator()(std::uintptr_t p) const {
      return std::hash<std::uintptr_t>()(p);
    }

    std::size_t operator()(const int* const& p) const {
      return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(p));
    }
  };

  struct equal_to_ptr {
    using is_transparent = std::true_type;

    bool operator()(const std::unique_ptr<int>& p1,
                    const std::unique_ptr<int>& p2) const {
      return p1 == p2;
    }

    bool operator()(const std::unique_ptr<int>& p1, std::uintptr_t p2) const {
      return reinterpret_cast<std::uintptr_t>(p1.get()) == p2;
    }

    bool operator()(std::uintptr_t p1, const std::unique_ptr<int>& p2) const {
      return p1 == reinterpret_cast<std::uintptr_t>(p2.get());
    }

    bool operator()(const std::unique_ptr<int>& p1,
                    const int* const& p2) const {
      return p1.get() == p2;
    }

    bool operator()(const int* const& p1,
                    const std::unique_ptr<int>& p2) const {
      return p1 == p2.get();
    }
  };

  std::unique_ptr<int> ptr1(new int(1));
  std::unique_ptr<int> ptr2(new int(2));
  std::unique_ptr<int> ptr3(new int(3));
  int other = -1;

  const std::uintptr_t addr1 = reinterpret_cast<std::uintptr_t>(ptr1.get());
  const int* const addr2 = ptr2.get();
  const int* const addr_unknown = &other;

  tsl::ordered_map<std::unique_ptr<int>, int, hash_ptr, equal_to_ptr> map;
  map.insert({std::move(ptr1), 4});
  map.insert({std::move(ptr2), 5});
  map.insert({std::move(ptr3), 6});

  BOOST_CHECK_EQUAL(map.size(), 3);

  BOOST_CHECK_EQUAL(map.at(addr1), 4);
  BOOST_CHECK_EQUAL(map.at(addr2), 5);
  TSL_OH_CHECK_THROW(map.at(addr_unknown), std::out_of_range);

  BOOST_REQUIRE(map.find(addr1) != map.end());
  BOOST_CHECK_EQUAL(*map.find(addr1)->first, 1);

  BOOST_REQUIRE(map.find(addr2) != map.end());
  BOOST_CHECK_EQUAL(*map.find(addr2)->first, 2);

  BOOST_CHECK(map.find(addr_unknown) == map.end());

  BOOST_CHECK_EQUAL(map.count(addr1), 1);
  BOOST_CHECK_EQUAL(map.count(addr2), 1);
  BOOST_CHECK_EQUAL(map.count(addr_unknown), 0);

  BOOST_CHECK_EQUAL(map.erase(addr1), 1);
  BOOST_CHECK_EQUAL(map.unordered_erase(addr2), 1);
  BOOST_CHECK_EQUAL(map.erase(addr_unknown), 0);
  BOOST_CHECK_EQUAL(map.unordered_erase(addr_unknown), 0);

  BOOST_CHECK_EQUAL(map.size(), 1);
}

/**
 * Various operations on empty map
 */
BOOST_AUTO_TEST_CASE(test_empty_map) {
  tsl::ordered_map<std::string, int> map(0);

  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.load_factor(), 0);
  BOOST_CHECK(map.empty());

  BOOST_CHECK(map.begin() == map.end());
  BOOST_CHECK(map.begin() == map.cend());
  BOOST_CHECK(map.cbegin() == map.cend());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map.find("test") == map.end());

  BOOST_CHECK_EQUAL(map.count(""), 0);
  BOOST_CHECK_EQUAL(map.count("test"), 0);

  BOOST_CHECK(!map.contains(""));
  BOOST_CHECK(!map.contains("test"));

  TSL_OH_CHECK_THROW(map.at(""), std::out_of_range);
  TSL_OH_CHECK_THROW(map.at("test"), std::out_of_range);

  auto range = map.equal_range("test");
  BOOST_CHECK(range.first == range.second);

  BOOST_CHECK_EQUAL(map.erase("test"), 0);
  BOOST_CHECK(map.erase(map.begin(), map.end()) == map.end());

  BOOST_CHECK_EQUAL(map["new value"], int{});
}

/**
 * Test precalculated hash
 */
BOOST_AUTO_TEST_CASE(test_precalculated_hash) {
  tsl::ordered_map<int, int, identity_hash<int>> map = {
      {1, -1}, {2, -2}, {3, -3}, {4, -4}, {5, -5}, {6, -6}};
  const tsl::ordered_map<int, int, identity_hash<int>> map_const = map;

  /**
   * find
   */
  BOOST_REQUIRE(map.find(3, map.hash_function()(3)) != map.end());
  BOOST_CHECK_EQUAL(map.find(3, map.hash_function()(3))->second, -3);

  BOOST_REQUIRE(map_const.find(3, map_const.hash_function()(3)) !=
                map_const.end());
  BOOST_CHECK_EQUAL(map_const.find(3, map_const.hash_function()(3))->second,
                    -3);

  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(3));
  BOOST_CHECK(map.find(3, map.hash_function()(2)) == map.end());

  /**
   * at
   */
  BOOST_CHECK_EQUAL(map.at(3, map.hash_function()(3)), -3);
  BOOST_CHECK_EQUAL(map_const.at(3, map_const.hash_function()(3)), -3);

  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(3));
  TSL_OH_CHECK_THROW(map.at(3, map.hash_function()(2)), std::out_of_range);

  /**
   * count
   */
  BOOST_CHECK(map.contains(3, map.hash_function()(3)));
  BOOST_CHECK(map_const.contains(3, map_const.hash_function()(3)));

  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(3));
  BOOST_CHECK(!map.contains(3, map.hash_function()(2)));

  /**
   * count
   */
  BOOST_CHECK_EQUAL(map.count(3, map.hash_function()(3)), 1);
  BOOST_CHECK_EQUAL(map_const.count(3, map_const.hash_function()(3)), 1);

  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(3));
  BOOST_CHECK_EQUAL(map.count(3, map.hash_function()(2)), 0);

  /**
   * equal_range
   */
  auto it_range = map.equal_range(3, map.hash_function()(3));
  BOOST_REQUIRE_EQUAL(std::distance(it_range.first, it_range.second), 1);
  BOOST_CHECK_EQUAL(it_range.first->second, -3);

  auto it_range_const = map_const.equal_range(3, map_const.hash_function()(3));
  BOOST_REQUIRE_EQUAL(
      std::distance(it_range_const.first, it_range_const.second), 1);
  BOOST_CHECK_EQUAL(it_range_const.first->second, -3);

  it_range = map.equal_range(3, map.hash_function()(2));
  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(3));
  BOOST_CHECK_EQUAL(std::distance(it_range.first, it_range.second), 0);

  /**
   * erase
   */
  BOOST_CHECK_EQUAL(map.erase(3, map.hash_function()(3)), 1);

  BOOST_REQUIRE_NE(map.hash_function()(2), map.hash_function()(4));
  BOOST_CHECK_EQUAL(map.erase(4, map.hash_function()(2)), 0);
}

BOOST_AUTO_TEST_SUITE_END()
