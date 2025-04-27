//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <functional>
#include <memory>

#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/cpp/detail/binding_handle.h"
#include "ten_runtime/binding/cpp/detail/msg/audio_frame.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/cmd.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd_result.h"
#include "ten_runtime/binding/cpp/detail/msg/data.h"
#include "ten_runtime/binding/cpp/detail/msg/video_frame.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/ten.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/internal/return.h"
#include "ten_runtime/ten_env/internal/send.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lang/cpp/lib/error.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_json.h"

using ten_env_t = struct ten_env_t;

namespace ten {

class app_t;
class extension_t;
class extension_group_t;
class addon_t;
class ten_env_t;
class ten_env_proxy_t;
class ten_env_internal_accessor_t;

using result_handler_func_t =
    std::function<void(ten_env_t &, std::unique_ptr<cmd_result_t>, error_t *)>;

using error_handler_func_t = std::function<void(ten_env_t &, error_t *)>;

class ten_env_t {
 public:
  // @{
  ten_env_t(const ten_env_t &) = delete;
  ten_env_t(ten_env_t &&) = delete;
  ten_env_t &operator=(const ten_env_t &) = delete;
  ten_env_t &operator=(const ten_env_t &&) = delete;
  // @}

  bool send_cmd(std::unique_ptr<cmd_t> &&cmd,
                result_handler_func_t &&result_handler = nullptr,
                error_t *err = nullptr) {
    return send_cmd_internal(std::move(cmd), std::move(result_handler), nullptr,
                             err);
  }

  // The differences between `send_cmd` and `send_cmd_ex` is that `send_cmd`
  // will only return the final `result` of `is_completed`. If other
  // behaviors are needed, users can use `send_cmd_ex`.
  bool send_cmd_ex(std::unique_ptr<cmd_t> &&cmd,
                   result_handler_func_t &&result_handler = nullptr,
                   error_t *err = nullptr) {
    ten_env_send_cmd_options_t options{
        .enable_multiple_results = true,
    };
    return send_cmd_internal(std::move(cmd), std::move(result_handler),
                             &options, err);
  }

  bool send_data(std::unique_ptr<data_t> &&data,
                 error_handler_func_t &&error_handler = nullptr,
                 error_t *err = nullptr) {
    return send_data_like_msg_internal<data_t>(
        std::move(data), std::move(error_handler), ten_env_send_data,
        proxy_handle_send_msg_error, err);
  }

  bool send_video_frame(std::unique_ptr<video_frame_t> &&frame,
                        error_handler_func_t &&error_handler = nullptr,
                        error_t *err = nullptr) {
    return send_data_like_msg_internal<video_frame_t>(
        std::move(frame), std::move(error_handler), ten_env_send_data,
        proxy_handle_send_msg_error, err);
  }

  bool send_audio_frame(std::unique_ptr<audio_frame_t> &&frame,
                        error_handler_func_t &&error_handler = nullptr,
                        error_t *err = nullptr) {
    return send_data_like_msg_internal<audio_frame_t>(
        std::move(frame), std::move(error_handler), ten_env_send_data,
        proxy_handle_send_msg_error, err);
  }

  bool return_result(std::unique_ptr<cmd_result_t> &&cmd,
                     error_handler_func_t &&error_handler = nullptr,
                     error_t *err = nullptr) {
    if (!cmd) {
      TEN_ASSERT(0, "Invalid argument.");
      return false;
    }

    bool rc = false;
    if (error_handler == nullptr) {
      rc = ten_env_return_result(c_ten_env, cmd->get_underlying_msg(), nullptr,
                                 nullptr,
                                 err != nullptr ? err->get_c_error() : nullptr);
    } else {
      auto *error_handler_ptr =
          new error_handler_func_t(std::move(error_handler));

      rc = ten_env_return_result(c_ten_env, cmd->get_underlying_msg(),
                                 proxy_handle_return_result_error,
                                 error_handler_ptr,
                                 err != nullptr ? err->get_c_error() : nullptr);
      if (!rc) {
        delete error_handler_ptr;
      }
    }

    if (rc) {
      // The 'cmd' has been returned, so we should release the ownership of
      // the C msg from the 'cmd'.
      auto *cpp_cmd_ptr = std::move(cmd).release();
      delete cpp_cmd_ptr;
    }

    return rc;
  }

  bool is_property_exist(const char *path, error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    if ((path == nullptr) || (strlen(path) == 0)) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "path should not be empty.");
      }
      return false;
    }

    return ten_env_is_property_exist(
        c_ten_env, path, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool init_property_from_json(const char *json_str, error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    if (json_str == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
      return false;
    }

    return ten_env_init_property_from_json(
        c_ten_env, json_str, err != nullptr ? err->get_c_error() : nullptr);
  }

  std::string get_property_to_json(const char *path, error_t *err = nullptr) {
    std::string result;

    auto *value = peek_property_value(path, err);
    if (value == nullptr) {
      return result;
    }

    ten_json_t c_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    bool success = ten_value_to_json(value, &c_json);
    if (!success) {
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

  bool set_property_from_json(const char *path, const char *json_str,
                              error_t *err = nullptr) {
    ten_json_t *c_json = ten_json_from_string(
        json_str, err != nullptr ? err->get_c_error() : nullptr);
    if (c_json == nullptr) {
      return false;
    }

    ten_value_t *value = ten_value_from_json(c_json);
    ten_json_destroy(c_json);

    return set_property_impl(path, value, err);
  }

  uint8_t get_property_uint8(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_uint8(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  uint16_t get_property_uint16(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_uint16(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  uint32_t get_property_uint32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_uint32(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  uint64_t get_property_uint64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_uint64(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  int8_t get_property_int8(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_int8(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  int16_t get_property_int16(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_int16(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  int32_t get_property_int32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_int32(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  int64_t get_property_int64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0;
    }

    error_t cpp_err;
    auto result = ten_value_get_int64(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  float get_property_float32(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0.0F;
    }

    error_t cpp_err;
    auto result = ten_value_get_float32(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  double get_property_float64(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return 0.0F;
    }

    error_t cpp_err;
    auto result = ten_value_get_float64(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  std::string get_property_string(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return "";
    }

    error_t cpp_err;
    const auto *result = ten_value_peek_raw_str(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    if (result != nullptr) {
      return result;
    } else {
      return "";
    }
  }

  void *get_property_ptr(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return nullptr;
    }

    error_t cpp_err;
    auto *result = ten_value_get_ptr(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
    }

    return result;
  }

  bool get_property_bool(const char *path, error_t *err = nullptr) {
    ten_value_t *c_value = peek_property_value(path, err);
    if (c_value == nullptr) {
      return false;
    }

    error_t cpp_err;
    auto result = ten_value_get_bool(c_value, cpp_err.get_c_error());
    if (!cpp_err.is_success()) {
      TEN_LOGW("Failed to get property %s because of incorrect type.", path);
    }
    if (err != nullptr) {
      ten_error_copy(err->get_c_error(), cpp_err.get_c_error());
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
    return set_property_impl(
        path, ten_value_create_ptr(value, nullptr, nullptr, nullptr), err);
  }

  bool set_property(const char *path, const char *value,
                    error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_string(value), err);
  }

  // Convenient overloaded function for std::string type.
  bool set_property(const char *path, const std::string &value,
                    error_t *err = nullptr) {
    return set_property_impl(path, ten_value_create_string(value.c_str()), err);
  }

  bool set_property(const char *path, const ten::buf_t &value,
                    error_t *err = nullptr) {
    ten_buf_t buf =
        TEN_BUF_STATIC_INIT_WITH_DATA_OWNED(value.data(), value.size());
    return set_property_impl(path, ten_value_create_buf_with_move(buf), err);
  }

  bool on_configure_done(error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    bool rc = ten_env_on_configure_done(
        c_ten_env, err != nullptr ? err->get_c_error() : nullptr);

    return rc;
  }

  bool on_init_done(error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    bool rc = ten_env_on_init_done(
        c_ten_env, err != nullptr ? err->get_c_error() : nullptr);

    return rc;
  }

  bool on_deinit_done(error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");
    return ten_env_on_deinit_done(
        c_ten_env, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool on_start_done(error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");
    return ten_env_on_start_done(c_ten_env,
                                 err != nullptr ? err->get_c_error() : nullptr);
  }

  bool on_stop_done(error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");
    return ten_env_on_stop_done(c_ten_env,
                                err != nullptr ? err->get_c_error() : nullptr);
  }

  bool on_create_instance_done(binding_handle_t *instance, void *context,
                               error_t *err = nullptr) {
    void *c_instance = nullptr;

    if (instance != nullptr) {
      c_instance = instance->get_c_instance();
      TEN_ASSERT(c_instance, "Should not happen.");
    }

    bool rc = ten_env_on_create_instance_done(
        c_ten_env, c_instance, context,
        err != nullptr ? err->get_c_error() : nullptr);

    return rc;
  }

  bool on_destroy_instance_done(void *context, error_t *err = nullptr) {
    bool rc = ten_env_on_destroy_instance_done(
        c_ten_env, context, err != nullptr ? err->get_c_error() : nullptr);

    return rc;
  }

#define TEN_ENV_LOG_VERBOSE(ten_env, msg)                                      \
  do {                                                                         \
    (ten_env).log(TEN_LOG_LEVEL_VERBOSE, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

#define TEN_ENV_LOG_DEBUG(ten_env, msg)                                      \
  do {                                                                       \
    (ten_env).log(TEN_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

#define TEN_ENV_LOG_INFO(ten_env, msg)                                      \
  do {                                                                      \
    (ten_env).log(TEN_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

#define TEN_ENV_LOG_WARN(ten_env, msg)                                      \
  do {                                                                      \
    (ten_env).log(TEN_LOG_LEVEL_WARN, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

#define TEN_ENV_LOG_ERROR(ten_env, msg)                                      \
  do {                                                                       \
    (ten_env).log(TEN_LOG_LEVEL_ERROR, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

#define TEN_ENV_LOG_FATAL(ten_env, msg)                                      \
  do {                                                                       \
    (ten_env).log(TEN_LOG_LEVEL_FATAL, __func__, __FILE__, __LINE__, (msg)); \
  } while (0)

  void log(TEN_LOG_LEVEL level, const char *func_name, const char *file_name,
           size_t line_no, const char *msg) {
    TEN_ASSERT(c_ten_env, "Should not happen.");
    ten_env_log(c_ten_env, level, func_name, file_name, line_no, msg);
  }

 private:
  friend class ten_env_proxy_t;
  friend class app_t;
  friend class extension_t;
  friend class extension_group_t;
  friend class addon_t;
  friend class addon_loader_t;
  friend class ten_env_internal_accessor_t;

  ::ten_env_t *c_ten_env;

  explicit ten_env_t(::ten_env_t *c_ten_env) : c_ten_env(c_ten_env) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    ten_binding_handle_set_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(c_ten_env),
        static_cast<void *>(this));
  }

  ~ten_env_t() { TEN_ASSERT(c_ten_env, "Should not happen."); }

  ::ten_env_t *get_c_ten_env() { return c_ten_env; }

  void *get_attached_target() {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    return ten_binding_handle_get_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(
            ten_env_get_attached_target(c_ten_env)));
  }

  bool init_manifest_from_json(const char *json_str, error_t *err);
  bool on_load_addon_done(void *context, error_t *err);

  bool send_cmd_internal(std::unique_ptr<cmd_t> &&cmd,
                         result_handler_func_t &&result_handler = nullptr,
                         ten_env_send_cmd_options_t *options = nullptr,
                         error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    bool rc = false;

    if (!cmd) {
      TEN_ASSERT(0, "Invalid argument.");
      return rc;
    }

    if (result_handler == nullptr) {
      rc = ten_env_send_cmd(c_ten_env, cmd->get_underlying_msg(), nullptr,
                            nullptr, options,
                            err != nullptr ? err->get_c_error() : nullptr);
    } else {
      auto *result_handler_ptr =
          new result_handler_func_t(std::move(result_handler));

      rc = ten_env_send_cmd(c_ten_env, cmd->get_underlying_msg(),
                            proxy_handle_send_cmd_result, result_handler_ptr,
                            options,
                            err != nullptr ? err->get_c_error() : nullptr);
      if (!rc) {
        delete result_handler_ptr;
      }
    }

    if (rc) {
      // Only when the cmd has been sent successfully, we should give back the
      // ownership of the cmd to the TEN runtime.
      auto *cpp_cmd_ptr = std::move(cmd).release();
      delete cpp_cmd_ptr;
    }

    return rc;
  }

  template <typename T>
  bool send_data_like_msg_internal(
      std::unique_ptr<T> &&msg, error_handler_func_t &&error_handler = nullptr,
      bool (*send_func)(::ten_env_t *, ten_shared_ptr_t *,
                        void (*)(::ten_env_t *, ten_shared_ptr_t *, void *,
                                 ten_error_t *),
                        void *, ten_error_t *) = nullptr,
      void (*proxy_func)(::ten_env_t *, ten_shared_ptr_t *, void *,
                         ten_error_t *) = nullptr,
      error_t *err = nullptr) {
    TEN_ASSERT(c_ten_env && msg, "Should not happen.");

    if (!msg || msg->get_underlying_msg() == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Invalid message.");
      }
      return false;
    }

    bool rc = false;

    if (error_handler == nullptr) {
      rc = send_func(c_ten_env, msg->get_underlying_msg(), nullptr, nullptr,
                     err != nullptr ? err->get_c_error() : nullptr);
    } else {
      auto *error_handler_ptr =
          new error_handler_func_t(std::move(error_handler));

      rc = send_func(c_ten_env, msg->get_underlying_msg(), proxy_func,
                     error_handler_ptr,
                     err != nullptr ? err->get_c_error() : nullptr);
      if (!rc) {
        delete error_handler_ptr;
      }
    }

    if (rc) {
      // Only when the msg has been sent successfully, we should give back the
      // ownership of the msg to the TEN runtime.
      auto *cpp_msg_ptr = std::move(msg).release();
      delete cpp_msg_ptr;
    }

    return rc;
  }

  ten_value_t *peek_property_value(const char *path, error_t *err) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    return ten_env_peek_property(c_ten_env, path,
                                 err != nullptr ? err->get_c_error() : nullptr);
  }

  /**
   * @note Note the move semantics of @a value. The @a value should not be
   * used after calling this function.
   */
  bool set_property_impl(const char *path, ten_value_t *value, error_t *err) {
    TEN_ASSERT(c_ten_env, "Should not happen.");

    bool rc = ten_env_set_property(
        c_ten_env, path, value, err != nullptr ? err->get_c_error() : nullptr);

    if (!rc) {
      ten_value_destroy(value);
    }
    return rc;
  }

  static void proxy_handle_send_cmd_result(::ten_env_t *ten_env,
                                           ten_shared_ptr_t *c_cmd_result,
                                           void *cb_data, ten_error_t *err) {
    auto *result_handler = static_cast<result_handler_func_t *>(cb_data);
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    auto cmd_result = cmd_result_t::create(
        // Clone a C shared_ptr to be owned by the C++ instance.
        ten_shared_ptr_clone(c_cmd_result));

    // After being processed by the `result_handler`, the `is_completed` value
    // of `cmd_result` may change. For example, if a command is passed between
    // two extensions within the same extension group (thread), the
    // `result_handler` in the source extension might directly invoke the
    // `on_cmd` logic of the destination extension. The logic within `on_cmd`
    // might then call `return_cmd`, which would cause the `is_completed` value
    // of the _same_ `cmd_result` to be modified. Therefore, before executing
    // the `result_handler`, the `is_completed` value needed for subsequent
    // decisions must be cached. After the `result_handler` has finished
    // executing, processing should be based on this cached value.
    bool is_completed = ten_cmd_result_is_completed(c_cmd_result, nullptr);

    if (err != nullptr) {
      error_t cpp_err(err, false);
      (*result_handler)(*cpp_ten_env, nullptr, &cpp_err);
    } else {
      (*result_handler)(*cpp_ten_env, std::move(cmd_result), nullptr);
    }

    if (is_completed) {
      // Only when is_final is true should the result handler be cleared.
      // Otherwise, since more result handlers are expected, the result
      // handler should not be cleared.
      delete result_handler;
    }
  }

  static void proxy_handle_send_msg_error(
      ::ten_env_t *ten_env, TEN_UNUSED ten_shared_ptr_t *c_cmd_result,
      void *cb_data, ten_error_t *err) {
    TEN_ASSERT(c_cmd_result == nullptr, "Should not happen.");

    auto *error_handler = static_cast<error_handler_func_t *>(cb_data);
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    if (err == nullptr) {
      (*error_handler)(*cpp_ten_env, nullptr);
    } else {
      error_t cpp_err(err, false);
      (*error_handler)(*cpp_ten_env, &cpp_err);
    }

    // The error handler should be cleared.
    delete error_handler;
  };

  static void proxy_handle_return_result_error(
      ::ten_env_t *ten_env, TEN_UNUSED ten_shared_ptr_t *c_cmd_result,
      void *user_data, ::ten_error_t *err) {
    TEN_ASSERT(ten_env, "Should not happen.");

    auto *error_handler = static_cast<error_handler_func_t *>(user_data);
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    if (err != nullptr) {
      error_t cpp_err(err, false);
      (*error_handler)(*cpp_ten_env, &cpp_err);
    } else {
      (*error_handler)(*cpp_ten_env, nullptr);
    }

    delete error_handler;
  }
};

}  // namespace ten
