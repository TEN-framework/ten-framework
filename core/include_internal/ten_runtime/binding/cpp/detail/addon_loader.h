//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>

#include "include_internal/ten_runtime/addon_loader/addon_loader.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/cpp/detail/binding_handle.h"
#include "ten_runtime/binding/cpp/detail/ten_env.h"
#include "ten_utils/macro/check.h"

using ten_addon_loader_t = struct ten_addon_loader_t;

namespace ten {

class addon_loader_t : public binding_handle_t {
 public:
  ~addon_loader_t() override {
    TEN_ASSERT(get_c_instance(), "Should not happen.");
    ten_addon_loader_destroy(
        static_cast<ten_addon_loader_t *>(get_c_instance()));

    TEN_ASSERT(cpp_ten_env, "Should not happen.");
    delete cpp_ten_env;
  }

  // @{
  addon_loader_t(const addon_loader_t &) = delete;
  addon_loader_t(addon_loader_t &&) = delete;
  addon_loader_t &operator=(const addon_loader_t &) = delete;
  addon_loader_t &operator=(const addon_loader_t &&) = delete;
  // @}

 protected:
  explicit addon_loader_t()
      : binding_handle_t(::ten_addon_loader_create(
            reinterpret_cast<ten_addon_loader_on_init_func_t>(&proxy_on_init),
            reinterpret_cast<ten_addon_loader_on_deinit_func_t>(
                &proxy_on_deinit),
            reinterpret_cast<ten_addon_loader_on_load_addon_func_t>(
                &proxy_on_load_addon))) {
    TEN_ASSERT(get_c_instance(), "Should not happen.");

    ten_binding_handle_set_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(get_c_instance()),
        static_cast<void *>(this));

    cpp_ten_env = new ten_env_t(ten_addon_loader_get_ten_env(
        static_cast<ten_addon_loader_t *>(get_c_instance())));
    TEN_ASSERT(cpp_ten_env, "Should not happen.");
  }

  virtual void on_init(ten_env_t &ten_env) = 0;

  virtual void on_deinit(ten_env_t &ten_env) = 0;

  // **Note:** This function, used to dynamically load other addons, may be
  // called from multiple threads. Therefore, it must be thread-safe.
  virtual void on_load_addon(ten_env_t &ten_env, TEN_ADDON_TYPE addon_type,
                             const char *addon_name, void *context) = 0;

 private:
  static void proxy_on_init(ten_addon_loader_t *addon_loader,
                            ::ten_env_t *ten_env) {
    TEN_ASSERT(addon_loader && ten_env, "Should not happen.");

    auto *cpp_addon_loader =
        static_cast<addon_loader_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon_loader)));

    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_addon_loader->invoke_cpp_addon_loader_on_init(*cpp_ten_env);
  }

  static void proxy_on_deinit(ten_addon_loader_t *addon_loader,
                              ::ten_env_t *ten_env) {
    TEN_ASSERT(addon_loader, "Should not happen.");

    auto *cpp_addon_loader =
        static_cast<addon_loader_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon_loader)));

    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_addon_loader->invoke_cpp_addon_loader_on_deinit(*cpp_ten_env);
  }

  static void proxy_on_load_addon(ten_addon_loader_t *addon_loader,
                                  ::ten_env_t *ten_env,
                                  TEN_ADDON_TYPE addon_type,
                                  const char *addon_name, void *context) {
    TEN_ASSERT(addon_loader, "Should not happen.");

    auto *cpp_addon_loader =
        static_cast<addon_loader_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon_loader)));

    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_addon_loader->invoke_cpp_addon_loader_on_load_addon(
        *cpp_ten_env, addon_type, addon_name, context);
  }

  void invoke_cpp_addon_loader_on_init(ten_env_t &ten_env) {
    try {
      on_init(ten_env);
    } catch (...) {
      TEN_ASSERT(0, "Should not happen.");
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }
  }

  void invoke_cpp_addon_loader_on_deinit(ten_env_t &ten_env) {
    try {
      on_deinit(ten_env);
    } catch (...) {
      TEN_ASSERT(0, "Should not happen.");
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }
  }

  void invoke_cpp_addon_loader_on_load_addon(ten_env_t &ten_env,
                                             TEN_ADDON_TYPE addon_type,
                                             const char *addon_name,
                                             void *context) {
    try {
      on_load_addon(ten_env, addon_type, addon_name, context);
    } catch (...) {
      TEN_ASSERT(0, "Should not happen.");
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }
  }

  ten_env_t *cpp_ten_env;
};

}  // namespace ten
