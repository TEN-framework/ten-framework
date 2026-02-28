//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ten {

struct bad_optional_access : public std::logic_error {
  bad_optional_access() : std::logic_error("bad optional access") {}
};

template <typename T>
class optional {
 public:
  optional() noexcept : engaged(false) {}
  // NOLINTNEXTLINE(hicpp-explicit-conversions, google-explicit-constructor)
  optional(std::nullptr_t) noexcept : engaged(false) {}

  // NOLINTNEXTLINE(hicpp-explicit-conversions, google-explicit-constructor)
  optional(const T &value) : engaged(true) { new (&storage) T(value); }
  // NOLINTNEXTLINE(hicpp-explicit-conversions, google-explicit-constructor)
  optional(T &&value) : engaged(true) { new (&storage) T(std::move(value)); }

  optional(const optional &other) : engaged(other.engaged) {
    if (engaged) {
      new (&storage) T(*other.data_ptr());
    }
  }

  optional &operator=(const optional &other) {
    if (this != &other) {
      if (engaged) {
        reset();
      }

      if (other.engaged) {
        new (&storage) T(*other.data_ptr());
        engaged = true;
      }
    }
    return *this;
  }

  optional(optional &&other) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : engaged(other.engaged) {
    if (engaged) {
      new (&storage) T(std::move(*other.data_ptr()));
      other.reset();
    }
  }

  optional &operator=(optional &&other) noexcept(
      std::is_nothrow_move_assignable<T>::value &&
      std::is_nothrow_move_constructible<T>::value) {
    if (this != &other) {
      if (engaged) {
        reset();
      }
      if (other.engaged) {
        new (&storage) T(std::move(*other.data_ptr()));
        engaged = true;
        other.reset();
      }
    }
    return *this;
  }

  ~optional() { reset(); }

  bool has_value() const noexcept { return engaged; }
  explicit operator bool() const noexcept { return has_value(); }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  T &operator*() {
    if (!engaged) {
      throw bad_optional_access();
    }
    return *data_ptr();
  }
  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  const T &operator*() const {
    if (!engaged) {
      throw bad_optional_access();
    }
    return *data_ptr();
  }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  T *operator->() { return &**this; }
  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  const T *operator->() const { return &**this; }

 private:
  bool engaged;
  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

  T *data_ptr() noexcept { return reinterpret_cast<T *>(&storage); }
  const T *data_ptr() const noexcept {
    return reinterpret_cast<const T *>(&storage);
  }

  void reset() noexcept {
    if (engaged) {
      data_ptr()->~T();
      engaged = false;
    }
  }
};

}  // namespace ten
