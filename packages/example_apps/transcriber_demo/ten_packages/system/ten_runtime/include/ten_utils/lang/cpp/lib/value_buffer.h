//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ten_utils/lang/cpp/lib/error.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/value/value_buffer.h"

namespace ten {
namespace value_buffer {

// Serialize a C++ value_t to a buffer for efficient cross-language transfer.
inline uint8_t *serialize_to_buffer(const value_t &value, size_t *buffer_size,
                                    error_t *err = nullptr) {
  if (!buffer_size) {
    if (err) {
      err->set_error_code(TEN_ERROR_CODE_INVALID_ARGUMENT);
      err->set_error_message("buffer_size parameter is required");
    }
    return nullptr;
  }

  // Get the underlying C value
  ten_value_t *c_value = value.get_c_value();
  if (!c_value) {
    if (err) {
      err->set_error_code(TEN_ERROR_CODE_INVALID_ARGUMENT);
      err->set_error_message("Invalid C++ value");
    }
    return nullptr;
  }

  ten_error_t *c_err = err ? err->get_c_error() : nullptr;

  // Call the C layer function
  uint8_t *buffer =
      ten_value_serialize_to_buffer_c(c_value, buffer_size, c_err);

  return buffer;
}

// Deserialize a buffer into a C++ value_t.
inline value_t deserialize_from_buffer(const uint8_t *buffer,
                                       size_t buffer_size,
                                       size_t *bytes_consumed = nullptr,
                                       error_t *err = nullptr) {
  if (!buffer || buffer_size == 0) {
    if (err) {
      err->set_error_code(TEN_ERROR_CODE_INVALID_ARGUMENT);
      err->set_error_message("Invalid buffer parameters");
    }
    return value_t();  // Return invalid value
  }

  ten_error_t *c_err = err ? err->get_c_error() : nullptr;

  // Call the C layer function
  ten_value_t *c_value = ten_value_deserialize_from_buffer_c(
      buffer, buffer_size, bytes_consumed, c_err);

  if (!c_value) {
    return value_t();  // Return invalid value
  }

  // Create C++ value wrapper that takes ownership of the C value
  return value_t(c_value, true);
}

// Serialize C++ native types to buffer.
template <typename T>
inline uint8_t *serialize_native_to_buffer(const T &native_value,
                                           size_t *buffer_size,
                                           error_t *err = nullptr) {
  // Create a temporary value_t from the native type
  value_t temp_value(native_value);
  return serialize_to_buffer(temp_value, buffer_size, err);
}

// Deserialize buffer to C++ native types.
template <typename T>
inline T deserialize_to_native(const uint8_t *buffer, size_t buffer_size,
                               size_t *bytes_consumed = nullptr,
                               error_t *err = nullptr) {
  value_t temp_value =
      deserialize_from_buffer(buffer, buffer_size, bytes_consumed, err);

  if (!temp_value.is_valid()) {
    return T{};  // Return default-constructed value
  }

  ten_error_t *c_err = err ? err->get_c_error() : nullptr;
  return temp_value.get_real_value<T>(c_err);
}

// Convenience functions for common C++ types

// Serialize std::unordered_map<std::string, T> to buffer.
template <typename T>
inline uint8_t *serialize_map_to_buffer(
    const std::unordered_map<std::string, T> &map, size_t *buffer_size,
    error_t *err = nullptr) {
  // Convert to std::map which is supported by value_t
  std::map<std::string, T> ordered_map(map.begin(), map.end());
  return serialize_native_to_buffer(ordered_map, buffer_size, err);
}

// Deserialize buffer to std::unordered_map<std::string, T>.
template <typename T>
inline std::unordered_map<std::string, T> deserialize_to_map(
    const uint8_t *buffer, size_t buffer_size, size_t *bytes_consumed = nullptr,
    error_t *err = nullptr) {
  // First deserialize to std::map
  std::map<std::string, T> ordered_map =
      deserialize_to_native<std::map<std::string, T>>(buffer, buffer_size,
                                                      bytes_consumed, err);

  // Convert to unordered_map
  return std::unordered_map<std::string, T>(ordered_map.begin(),
                                            ordered_map.end());
}

// Serialize std::vector<T> to buffer.
template <typename T>
inline uint8_t *serialize_vector_to_buffer(const std::vector<T> &vec,
                                           size_t *buffer_size,
                                           error_t *err = nullptr) {
  return serialize_native_to_buffer(vec, buffer_size, err);
}

// Deserialize buffer to std::vector<T>.
template <typename T>
inline std::vector<T> deserialize_to_vector(const uint8_t *buffer,
                                            size_t buffer_size,
                                            size_t *bytes_consumed = nullptr,
                                            error_t *err = nullptr) {
  return deserialize_to_native<std::vector<T>>(buffer, buffer_size,
                                               bytes_consumed, err);
}

}  // namespace value_buffer
}  // namespace ten
