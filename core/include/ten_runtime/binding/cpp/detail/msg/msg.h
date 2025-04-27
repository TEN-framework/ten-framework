//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <string>

#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/lang/cpp/lib/error.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_json.h"

namespace ten {

class ten_env_t;
class cmd_result_t;
class msg_t;
class msg_internal_accessor_t;

class msg_t {
 public:
  virtual ~msg_t() { relinquish_underlying_msg(); }

  // @{
  msg_t(msg_t &other) = delete;
  msg_t(msg_t &&other) = delete;
  msg_t &operator=(const msg_t &other) = delete;
  msg_t &operator=(msg_t &&other) noexcept = delete;
  // @}

  explicit operator bool() const { return c_msg != nullptr; }

  std::string get_name(error_t *err = nullptr) const {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_GENERIC,
                      "Invalid TEN message.");
      }
      return "";
    }

    return ten_msg_get_name(c_msg);
  }

  bool set_dest(const char *uri, const char *graph,
                const char *extension_group_name, const char *extension_name,
                error_t *err = nullptr) const {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return false;
    }

    return ten_msg_clear_and_set_dest(
        c_msg, uri, graph, extension_group_name, extension_name,
        err != nullptr ? err->get_c_error() : nullptr);
  }

  bool is_property_exist(const char *path, error_t *err = nullptr) {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return false;
    }

    TEN_ASSERT(path && strlen(path), "path should not be empty.");

    return ten_msg_is_property_exist(
        c_msg, path, err != nullptr ? err->get_c_error() : nullptr);
  }

  uint8_t get_property_uint8(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_uint8(c_value,
                               err != nullptr ? err->get_c_error() : nullptr);
  }

  uint16_t get_property_uint16(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_uint16(c_value,
                                err != nullptr ? err->get_c_error() : nullptr);
  }

  uint32_t get_property_uint32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_uint32(c_value,
                                err != nullptr ? err->get_c_error() : nullptr);
  }

  uint64_t get_property_uint64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_uint64(c_value,
                                err != nullptr ? err->get_c_error() : nullptr);
  }

  int8_t get_property_int8(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_int8(c_value,
                              err != nullptr ? err->get_c_error() : nullptr);
  }

  int16_t get_property_int16(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_int16(c_value,
                               err != nullptr ? err->get_c_error() : nullptr);
  }

  int32_t get_property_int32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_int32(c_value,
                               err != nullptr ? err->get_c_error() : nullptr);
  }

  int64_t get_property_int64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }
    return ten_value_get_int64(c_value,
                               err != nullptr ? err->get_c_error() : nullptr);
  }

  float get_property_float32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0.0F;
    }
    return ten_value_get_float32(c_value,
                                 err != nullptr ? err->get_c_error() : nullptr);
  }

  double get_property_float64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0.0F;
    }
    return ten_value_get_float64(c_value,
                                 err != nullptr ? err->get_c_error() : nullptr);
  }

  std::string get_property_string(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return "";
    }
    return ten_value_peek_raw_str(
        c_value, err != nullptr ? err->get_c_error() : nullptr);
  }

  void *get_property_ptr(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return nullptr;
    }
    return ten_value_get_ptr(c_value,
                             err != nullptr ? err->get_c_error() : nullptr);
  }

  bool get_property_bool(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return false;
    }
    return ten_value_get_bool(c_value,
                              err != nullptr ? err->get_c_error() : nullptr);
  }

  // Pay attention to its copy semantics.
  ten::buf_t get_property_buf(const char *path, error_t *err = nullptr) {
    auto result = ten::buf_t{};

    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return result;
    }

    ten_buf_t *c_buf = ten_value_peek_buf(
        c_value, err != nullptr ? err->get_c_error() : nullptr);
    if (c_buf == nullptr) {
      return result;
    }

    ten_buf_init_with_copying_data(&result.buf, c_buf->data, c_buf->size);

    return result;
  }

  std::string get_property_to_json(const char *path = nullptr,
                                   error_t *err = nullptr) const {
    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return "";
    }

    std::string result;

    auto *value = peek_property_value(path, err);
    if (value == nullptr) {
      return result;
    }

    ten_json_t c_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    bool success = ten_value_to_json(value, &c_json);
    if (!success) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_GENERIC,
                      "Invalid TEN message.");
      }
      return result;
    }

    bool must_free = false;
    const char *json_str = ten_json_to_string(&c_json, nullptr, &must_free);
    TEN_ASSERT(json_str, "Failed to convert a JSON to a string");

    result = json_str;

    ten_json_deinit(&c_json);
    if (must_free) {
      TEN_FREE(json_str);
    }

    return result;
  }

  bool set_property(const char *path, int8_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_int8(value), err);
  }

  bool set_property(const char *path, int16_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_int16(value), err);
  }

  bool set_property(const char *path, int32_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_int32(value), err);
  }

  bool set_property(const char *path, int64_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_int64(value), err);
  }

  bool set_property(const char *path, uint8_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_uint8(value), err);
  }

  bool set_property(const char *path, uint16_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_uint16(value), err);
  }

  bool set_property(const char *path, uint32_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_uint32(value), err);
  }

  bool set_property(const char *path, uint64_t value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_uint64(value), err);
  }

  bool set_property(const char *path, float value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_float32(value), err);
  }

  bool set_property(const char *path, double value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_float64(value), err);
  }

  bool set_property(const char *path, bool value, error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_bool(value), err);
  }

  bool set_property(const char *path, void *value, error_t *err = nullptr) {
    if (value == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid argment.");
      }
      return false;
    }
    return set_property_impl(
        path, ten_value_create_ptr(value, nullptr, nullptr, nullptr), err);
  }

  bool set_property(const char *path, const char *value,
                    error_t *err = nullptr) {
    if (value == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid argment.");
      }
      return false;
    }
    return set_property_impl(path, ten_value_create_string(value), err);
  }

  bool set_property(const char *path, const std::string &value,
                    error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_string(value.c_str()), err);
  }

  // Pay attention to its copy semantics.
  bool set_property(const char *path, const ten::buf_t &value,
                    error_t *err = nullptr) {
    if (value.data() == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid argment.");
      }
      return false;
    }
    ten_buf_t buf;
    ten_buf_init_with_copying_data(&buf, value.data(), value.size());
    return set_property_impl(path, ten_value_create_buf_with_move(buf), err);
  }

  bool set_property_from_json(const char *path, const char *json,
                              error_t *err = nullptr) {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_GENERIC,
                      "Invalid TEN message.");
      }
      return false;
    }

    ten_json_t *c_json = ten_json_from_string(
        json, err != nullptr ? err->get_c_error() : nullptr);
    if (c_json == nullptr) {
      return false;
    }

    ten_value_t *value = ten_value_from_json(c_json);
    ten_json_destroy(c_json);

    return set_property_impl(path, value, err);
  }

  // Internal use only.
  ten_shared_ptr_t *get_underlying_msg() const { return c_msg; }

 protected:
  friend class ten_env_t;
  friend class cmd_result_t;

  // @{
  // Used by the constructor of the real command class to create a base command
  // first.
  msg_t() = default;
  explicit msg_t(ten_shared_ptr_t *msg) : c_msg(msg) {}
  // @}

  /**
   * @note Note the move semantics of @a value. The @a value should not be
   * used after calling this function.
   */
  bool set_property_impl(const char *path, ten_value_t *value,
                         error_t *err = nullptr) {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return false;
    }

    bool rc = ten_msg_set_property(
        c_msg, path, value, err != nullptr ? err->get_c_error() : nullptr);

    if (!rc) {
      ten_value_destroy(value);
    }
    return rc;
  }

  void relinquish_underlying_msg() {
    if (c_msg != nullptr) {
      ten_shared_ptr_destroy(c_msg);
      c_msg = nullptr;
    }
  }

  ten_shared_ptr_t *c_msg = nullptr;  // NOLINT

 private:
  friend msg_internal_accessor_t;

  ten_value_t *peek_property_value(const char *path, error_t *err) const {
    TEN_ASSERT(c_msg, "Should not happen.");

    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return nullptr;
    }

    return ten_msg_peek_property(c_msg, path,
                                 err != nullptr ? err->get_c_error() : nullptr);
  }

  // Not sure if we should have this as a public API, so for now, let's keep it
  // private.
  TEN_MSG_TYPE get_type(error_t *err = nullptr) const {
    if (c_msg == nullptr) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid TEN message.");
      }
      return TEN_MSG_TYPE_INVALID;
    }

    return ten_msg_get_type(c_msg);
  }
};

// NOLINTEND(cert-dcl50-cpp)

}  // namespace ten
