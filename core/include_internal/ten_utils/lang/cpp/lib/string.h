//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#if defined(__EXCEPTIONS)
#include <stdexcept>
#endif
#include <memory>
#include <string>
#include <vector>

#include "ten_utils/lib/string.h"

namespace ten {

template <typename... Args>
std::string cpp_string_format(const std::string &format, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) +
               1;  // Extra space for '\0'
  if (size_s <= 0) {
#if defined(__EXCEPTIONS)
    throw std::runtime_error("Error during formatting.");
#else
    return "";
#endif
  }
  auto size = static_cast<size_t>(size_s);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(),
                     buf.get() + size - 1);  // We don't want the '\0' inside
}

static inline std::string cpp_string_uri_encode(const std::string &str) {
  ten_string_t uri_encoded;
  TEN_STRING_INIT(uri_encoded);

  ten_c_string_uri_encode(str.c_str(), str.length(), &uri_encoded);
  std::string result = ten_string_get_raw_str(&uri_encoded);

  ten_string_deinit(&uri_encoded);
  return result;
}

static inline std::string cpp_string_uri_decode(const std::string &str) {
  ten_string_t uri_decoded;
  TEN_STRING_INIT(uri_decoded);

  ten_c_string_uri_decode(str.c_str(), str.length(), &uri_decoded);
  std::string result = ten_string_get_raw_str(&uri_decoded);

  ten_string_deinit(&uri_decoded);
  return result;
}

static inline std::string cpp_string_escaped(const std::string &str) {
  ten_string_t escaped_str;
  TEN_STRING_INIT(escaped_str);

  ten_c_string_escaped(str.c_str(), &escaped_str);
  std::string result = ten_string_get_raw_str(&escaped_str);

  ten_string_deinit(&escaped_str);
  return result;
}

static inline std::vector<std::string> cpp_string_split(
    std::string str, std::string delimiter, bool keep_empty = true) {
  size_t pos_start = 0;
  size_t pos_end;
  size_t delim_len = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = str.find(delimiter, pos_start)) != std::string::npos) {
    token = str.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    if (keep_empty || !token.empty()) {
      res.push_back(token);
    }
  }

  token = str.substr(pos_start);
  if (keep_empty || !token.empty()) {
    res.push_back(token);
  }
  return res;
}

class TenString {
 public:
  TenString() : str_(nullptr) {}

  TenString(const char *str)
      : str_(str != nullptr ? ten_string_create_formatted(str) : nullptr) {}

  TenString(const std::string &str)
      : str_((!str.empty()) ? ten_string_create_formatted(str.c_str())
                            : nullptr) {}

  TenString(const ten_string_t *str) : str_(const_cast<ten_string_t *>(str)) {}

  TenString(const TenString &rhs) : str_(nullptr) { operator=(rhs); }

  TenString(TenString &&rhs) noexcept : str_(nullptr) {
    operator=(std::move(rhs));
  }

  TenString &operator=(const TenString &rhs) {
    if (this == &rhs) {
      return *this;
    }

    if (str_ != nullptr) {
      ten_string_destroy(str_);
    }

    str_ = nullptr;
    if (rhs.str_ != nullptr) {
      str_ = ten_string_clone(rhs.str_);
    }

    return *this;
  }

  TenString &operator=(TenString &&rhs) noexcept {
    if (this == &rhs) {
      return *this;
    }

    if (str_ != nullptr) {
      ten_string_destroy(str_);
    }

    str_ = rhs.str_;
    rhs.str_ = nullptr;
    return *this;
  }

  operator const ten_string_t *() const { return str_; }

  bool operator==(const TenString &rhs) const {
    if (str_ == nullptr && rhs.str_ == nullptr) {
      return true;
    }

    if (str_ == nullptr && rhs.str_ != nullptr) {
      return false;
    }

    if (str_ != nullptr && rhs.str_ == nullptr) {
      return false;
    }

    return ten_string_is_equal(str_, rhs.str_);
  }

  bool operator==(const ten_string_t *rhs) const {
    if (str_ == nullptr && rhs == nullptr) {
      return true;
    }

    if (str_ == nullptr && rhs != nullptr) {
      return false;
    }

    if (str_ != nullptr && rhs == nullptr) {
      return false;
    }

    return ten_string_is_equal(str_, rhs);
  }

  bool operator==(const char *rhs) const {
    if (str_ == nullptr && rhs == nullptr) {
      return true;
    }

    if (str_ == nullptr && rhs != nullptr) {
      return false;
    }

    if (str_ != nullptr && rhs == nullptr) {
      return false;
    }

    return ten_string_is_equal_c_str(str_, rhs);
  }

  bool operator==(const std::string &rhs) const {
    if (str_ == nullptr && rhs.empty()) {
      return true;
    }

    if (str_ == nullptr && !rhs.empty()) {
      return false;
    }

    if (str_ != nullptr && rhs.empty()) {
      return false;
    }

    return ten_string_is_equal_c_str(str_, rhs.c_str());
  }

  bool operator!=(const TenString &rhs) const { return !operator==(rhs); }

  bool operator!=(const ten_string_t *rhs) const { return !operator==(rhs); }

  bool operator!=(const char *rhs) const { return !operator==(rhs); }

  bool operator!=(const std::string &rhs) const { return !operator==(rhs); }

  TenString &operator+=(const TenString &rhs) {
    if (rhs.empty()) {
      return *this;
    }

    if (empty()) {
      str_ = ten_string_clone(rhs.str_);
      return *this;
    }

    ten_string_append_formatted(str_, rhs.str_->buf);
    return *this;
  }

  TenString &operator+=(const ten_string_t *rhs) {
    if (rhs == nullptr || ten_string_is_empty(rhs)) {
      return *this;
    }

    if (empty()) {
      str_ = ten_string_clone(const_cast<ten_string_t *>(rhs));
      return *this;
    }

    ten_string_append_formatted(str_, rhs->buf);
    return *this;
  }

  TenString &operator+=(const char *rhs) {
    if (rhs == nullptr || *rhs == '\0') {
      return *this;
    }

    if (empty()) {
      str_ = ten_string_create_formatted(rhs);
      return *this;
    }

    ten_string_append_formatted(str_, rhs);
    return *this;
  }

  TenString &operator+=(const std::string &rhs) {
    if (rhs.empty()) {
      return *this;
    }

    if (empty()) {
      str_ = ten_string_create_formatted(rhs.c_str());
      return *this;
    }

    ten_string_append_formatted(str_, rhs.c_str());
    return *this;
  }

  TenString operator+(const TenString &rhs) const {
    TenString result(*this);
    result += rhs;
    return result;
  }

  TenString operator+(const ten_string_t *rhs) const {
    TenString result(*this);
    result += rhs;
    return result;
  }

  TenString operator+(const char *rhs) const {
    TenString result(*this);
    result += rhs;
    return result;
  }

  TenString operator+(const std::string &rhs) const {
    TenString result(*this);
    result += rhs;
    return result;
  }

  bool empty() const { return str_ == nullptr || ten_string_is_empty(str_); }

  ~TenString() {
    if (str_ != nullptr) {
      ten_string_destroy(str_);
    }
  }

  const char *c_str() const { return str_ ? str_->buf : nullptr; }

  size_t size() const { return str_ ? ten_string_len(str_) : 0; }

 private:
  ten_string_t *str_;
};

}  // namespace ten
