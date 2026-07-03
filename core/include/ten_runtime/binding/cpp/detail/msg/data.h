//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <cstddef>
#include <cstring>
#include <memory>

#include "ten_runtime/binding/cpp/detail/msg/msg.h"
#include "ten_runtime/msg/data/data.h"
#include "ten_utils/lang/cpp/lib/buf.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class ten_env_t;
class extension_t;
class ten_env_t;

class data_t : public msg_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend data_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  static std::unique_ptr<data_t> create(const char *data_name,
                                        error_t *err = nullptr) {
    if (data_name == nullptr || strlen(data_name) == 0) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Data name cannot be empty.");
      }
      return nullptr;
    }

    auto *c_data = ten_data_create(
        data_name, err != nullptr ? err->get_c_error() : nullptr);

    return std::make_unique<data_t>(c_data, ctor_passkey_t());
  }

  explicit data_t(ten_shared_ptr_t *data, ctor_passkey_t /*unused*/)
      : msg_t(data) {}

  ~data_t() override = default;

  std::unique_ptr<data_t> clone() const {
    if (c_msg == nullptr) {
      TEN_ASSERT(0, "Should not happen.");
      return nullptr;
    }

    ten_shared_ptr_t *cloned_msg = ten_msg_clone(c_msg, nullptr);
    if (cloned_msg == nullptr) {
      return nullptr;
    }

    return std::make_unique<data_t>(cloned_msg, ctor_passkey_t());
  }

  bool alloc_buf(size_t size, error_t *err = nullptr) {
    return ten_data_alloc_buf(c_msg, size) != nullptr;
  }

  buf_t lock_buf(error_t *err = nullptr) {
    if (!ten_msg_add_locked_res_buf(
            c_msg, ten_data_peek_buf(c_msg)->data,
            err != nullptr ? err->get_c_error() : nullptr)) {
      return {};
    }

    buf_t result{ten_data_peek_buf(c_msg)->data,
                 ten_data_peek_buf(c_msg)->size};

    return result;
  }

  bool unlock_buf(buf_t &buf, error_t *err = nullptr) {
    if (!ten_msg_remove_locked_res_buf(
            c_msg, buf.data(), err != nullptr ? err->get_c_error() : nullptr)) {
      return false;
    }

    // Since the `buf` has already been given back, clearing the contents of the
    // `buf` itself not only notifies developers that this `buf` can no longer
    // be used, but also prevents it from being used incorrectly again.
    ten_buf_init_with_owned_data(&buf.buf, 0);

    return true;
  }

  // Pay attention to its copy semantics.
  buf_t get_buf(error_t *err = nullptr) const {
    size_t data_size = ten_data_peek_buf(c_msg)->size;

    buf_t buf{data_size};
    if (data_size != 0) {
      memcpy(buf.data(), ten_data_peek_buf(c_msg)->data, data_size);
    }
    return buf;
  }

  // @{
  data_t(data_t &other) = delete;
  data_t(data_t &&other) = delete;
  data_t &operator=(data_t &other) = delete;
  data_t &operator=(data_t &&other) = delete;
  // @}

  // @{
  // Internal use only. This function is called in 'extension_t' to create C++
  // message from C message.
  explicit data_t(ten_shared_ptr_t *data) : msg_t(data) {}
  // @}

 private:
  friend ten_env_t;

  static std::unique_ptr<data_t> create(ten_shared_ptr_t *data,
                                        error_t *err = nullptr) {
    return std::make_unique<data_t>(data, ctor_passkey_t());
  }
};

}  // namespace ten
