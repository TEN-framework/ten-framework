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
#ifndef TSL_UTILS_H
#define TSL_UTILS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "tsl/ordered_hash.h"

#ifdef TSL_OH_NO_EXCEPTIONS
#define TSL_OH_CHECK_THROW(S, E)
#else
#define TSL_OH_CHECK_THROW(S, E) BOOST_CHECK_THROW(S, E)
#endif

template <typename T>
class identity_hash {
 public:
  std::size_t operator()(const T& value) const {
    return static_cast<std::size_t>(value);
  }
};

template <unsigned int MOD>
class mod_hash {
 public:
  template <typename T>
  std::size_t operator()(const T& value) const {
    return std::hash<T>()(value) % MOD;
  }
};

class move_only_test {
 public:
  explicit move_only_test(std::int64_t value)
      : m_value(new std::int64_t(value)) {}

  move_only_test(const move_only_test&) = delete;
  move_only_test(move_only_test&&) = default;
  move_only_test& operator=(const move_only_test&) = delete;
  move_only_test& operator=(move_only_test&&) = default;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const move_only_test& value) {
    if (value.m_value == nullptr) {
      stream << "null";
    } else {
      stream << *value.m_value;
    }

    return stream;
  }

  friend bool operator==(const move_only_test& lhs, const move_only_test& rhs) {
    if (lhs.m_value == nullptr || rhs.m_value == nullptr) {
      return lhs.m_value == nullptr && rhs.m_value == nullptr;
    } else {
      return *lhs.m_value == *rhs.m_value;
    }
  }

  friend bool operator!=(const move_only_test& lhs, const move_only_test& rhs) {
    return !(lhs == rhs);
  }

  std::int64_t value() const { return *m_value; }

 private:
  std::unique_ptr<std::int64_t> m_value;
};

class copy_constructible_only_test {
 public:
  explicit copy_constructible_only_test(std::int64_t value) : m_value(value) {}

  copy_constructible_only_test(const copy_constructible_only_test&) = default;
  copy_constructible_only_test(copy_constructible_only_test&&) = delete;
  copy_constructible_only_test& operator=(const copy_constructible_only_test&) =
      delete;
  copy_constructible_only_test& operator=(copy_constructible_only_test&&) =
      delete;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const copy_constructible_only_test& value) {
    stream << value.m_value;
    return stream;
  }

  friend bool operator==(const copy_constructible_only_test& lhs,
                         const copy_constructible_only_test& rhs) {
    return lhs.m_value == rhs.m_value;
  }

  friend bool operator!=(const copy_constructible_only_test& lhs,
                         const copy_constructible_only_test& rhs) {
    return !(lhs == rhs);
  }

  std::int64_t value() const { return m_value; }

 private:
  std::int64_t m_value;
};

namespace std {
template <>
struct hash<move_only_test> {
  std::size_t operator()(const move_only_test& val) const {
    return std::hash<std::int64_t>()(val.value());
  }
};

template <>
struct hash<copy_constructible_only_test> {
  std::size_t operator()(const copy_constructible_only_test& val) const {
    return std::hash<std::int64_t>()(val.value());
  }
};
}  // namespace std

class utils {
 public:
  template <typename T>
  static T get_key(std::size_t counter);

  template <typename T>
  static T get_value(std::size_t counter);

  template <typename HMap>
  static HMap get_filled_hash_map(std::size_t nb_elements);

  /**
   * The ordered_map equality operator only compares the m_values structure as
   * it is sufficient for ensuring equality. This method do a more extensive
   * comparison to ensure that the internal state of the map is coherent.
   */
  template <typename HMap>
  static bool test_is_equal(const HMap& lhs, const HMap& rhs) {
    if (lhs != rhs) {
      return false;
    }

    if(lhs.size() != rhs.size()) {
      return false;
    }

    for (const auto& val_lhs : lhs) {
      auto it_rhs = rhs.find(val_lhs.first);
      if (it_rhs == rhs.end()) {
        return false;
      }

      if (val_lhs.first != it_rhs->first) {
        return false;
      }

      if (val_lhs.second != it_rhs->second) {
        return false;
      }
    }

    for (const auto& val_rhs : rhs) {
      auto it_lhs = lhs.find(val_rhs.first);
      if (it_lhs == lhs.end()) {
        return false;
      }

      if (it_lhs->first != val_rhs.first) {
        return false;
      }

      if (it_lhs->second != val_rhs.second) {
        return false;
      }
    }

    return true;
  }
};

template <>
inline std::int64_t utils::get_key<std::int64_t>(std::size_t counter) {
  return tsl::detail_ordered_hash::numeric_cast<std::int64_t>(counter);
}

template <>
inline std::string utils::get_key<std::string>(std::size_t counter) {
  return "Key " + std::to_string(counter);
}

template <>
inline move_only_test utils::get_key<move_only_test>(std::size_t counter) {
  return move_only_test(
      tsl::detail_ordered_hash::numeric_cast<std::int64_t>(counter));
}

template <>
inline std::int64_t utils::get_value<std::int64_t>(std::size_t counter) {
  return tsl::detail_ordered_hash::numeric_cast<std::int64_t>(counter * 2);
}

template <>
inline std::string utils::get_value<std::string>(std::size_t counter) {
  return "Value " + std::to_string(counter);
}

template <>
inline move_only_test utils::get_value<move_only_test>(std::size_t counter) {
  return move_only_test(
      tsl::detail_ordered_hash::numeric_cast<std::int64_t>(counter * 2));
}

template <typename HMap>
inline HMap utils::get_filled_hash_map(std::size_t nb_elements) {
  using key_tt = typename HMap::key_type;
  using value_tt = typename HMap::mapped_type;

  HMap map;
  map.reserve(nb_elements);

  for (std::size_t i = 0; i < nb_elements; i++) {
    map.insert({utils::get_key<key_tt>(i), utils::get_value<value_tt>(i)});
  }

  return map;
}

template <class T>
struct is_pair : std::false_type {};

template <class T1, class T2>
struct is_pair<std::pair<T1, T2>> : std::true_type {};

/**
 * serializer helper to test serialize(...) and deserialize(...) functions
 */
class serializer {
 public:
  serializer() { m_ostream.exceptions(m_ostream.badbit | m_ostream.failbit); }

  template <class T>
  void operator()(const T& val) {
    serialize_impl(val);
  }

  std::string str() const { return m_ostream.str(); }

 private:
  template <typename T, typename U>
  void serialize_impl(const std::pair<T, U>& val) {
    serialize_impl(val.first);
    serialize_impl(val.second);
  }

  void serialize_impl(const std::string& val) {
    serialize_impl(
        tsl::detail_ordered_hash::numeric_cast<std::uint64_t>(val.size()));
    m_ostream.write(val.data(), val.size());
  }

  void serialize_impl(const move_only_test& val) {
    serialize_impl(val.value());
  }

  template <class T, typename std::enable_if<
                         std::is_arithmetic<T>::value>::type* = nullptr>
  void serialize_impl(const T& val) {
    m_ostream.write(reinterpret_cast<const char*>(&val), sizeof(val));
  }

 private:
  std::stringstream m_ostream;
};

class deserializer {
 public:
  explicit deserializer(const std::string& init_str = "")
      : m_istream(init_str) {
    m_istream.exceptions(m_istream.badbit | m_istream.failbit |
                         m_istream.eofbit);
  }

  template <class T>
  T operator()() {
    return deserialize_impl<T>();
  }

 private:
  template <class T,
            typename std::enable_if<is_pair<T>::value>::type* = nullptr>
  T deserialize_impl() {
    auto first = deserialize_impl<typename T::first_type>();
    return std::make_pair(std::move(first),
                          deserialize_impl<typename T::second_type>());
  }

  template <class T, typename std::enable_if<
                         std::is_same<std::string, T>::value>::type* = nullptr>
  T deserialize_impl() {
    const std::size_t str_size =
        tsl::detail_ordered_hash::numeric_cast<std::size_t>(
            deserialize_impl<std::uint64_t>());

    // TODO std::string::data() return a const pointer pre-C++17. Avoid the
    // inefficient double allocation.
    std::vector<char> chars(str_size);
    m_istream.read(chars.data(), str_size);

    return std::string(chars.data(), chars.size());
  }

  template <class T, typename std::enable_if<std::is_same<
                         move_only_test, T>::value>::type* = nullptr>
  move_only_test deserialize_impl() {
    return move_only_test(deserialize_impl<std::int64_t>());
  }

  template <class T, typename std::enable_if<
                         std::is_arithmetic<T>::value>::type* = nullptr>
  T deserialize_impl() {
    T val;
    m_istream.read(reinterpret_cast<char*>(&val), sizeof(val));

    return val;
  }

 private:
  std::stringstream m_istream;
};

#endif
