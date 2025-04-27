//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <cstddef>
#include <string>

#include "ten_runtime/app/app.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/cpp/detail/common.h"
#include "ten_runtime/binding/cpp/detail/ten_env.h"
#include "ten_runtime/ten.h"
#include "ten_utils/macro/check.h"

using ten_app_t = struct ten_app_t;

namespace ten {

class app_t {
 public:
  app_t()
      : c_app(ten_app_create(cpp_app_on_configure_cb_wrapper,
                             cpp_app_on_init_cb_wrapper, nullptr, nullptr)),
        cpp_ten_env(new ten_env_t(ten_app_get_ten_env(c_app))) {
    TEN_ASSERT(cpp_ten_env, "Should not happen.");
    ten_binding_handle_set_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(c_app),
        static_cast<void *>(this));
  }

  virtual ~app_t() {
    ten_app_destroy(c_app);
    c_app = nullptr;

    TEN_ASSERT(cpp_ten_env, "Should not happen.");
    delete cpp_ten_env;
  }

  // @{
  app_t(const app_t &) = delete;
  app_t(app_t &&) = delete;
  app_t &operator=(const app_t &) = delete;
  app_t &operator=(app_t &&) = delete;
  // @}

  bool run(bool run_in_background = false, error_t *err = nullptr) {
    if (c_app == nullptr) {
      return false;
    }

    return ten_app_run(c_app, run_in_background,
                       err != nullptr ? err->get_c_error() : nullptr);
  }

  bool close(error_t *err = nullptr) {
    return ten_app_close(c_app, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool wait(error_t *err = nullptr) {
    return ten_app_wait(c_app, err != nullptr ? err->get_c_error() : nullptr);
  }

 protected:
  virtual void on_configure(ten_env_t &ten_env) { ten_env.on_configure_done(); }

  virtual void on_init(ten_env_t &ten_env) { ten_env.on_init_done(); }

  virtual void on_deinit(ten_env_t &ten_env) { ten_env.on_deinit_done(); }

 private:
  static void cpp_app_on_configure_cb_wrapper(ten_app_t *app,
                                              ::ten_env_t *ten_env) {
    TEN_ASSERT(app, "Should not happen.");
    TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
    TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, true),
               "Should not happen.");
    TEN_ASSERT(ten_app_get_ten_env(app) == ten_env, "Should not happen.");

    auto *cpp_app =
        static_cast<app_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(app)));
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_app->on_configure_helper_for_cpp(*cpp_ten_env);
  }

  void on_configure_helper_for_cpp(ten_env_t &ten_env) {
    // The TEN runtime does not use C++ exceptions. The use of try/catch here is
    // merely to intercept any exceptions that might be thrown by the user's app
    // code. If exceptions are disabled during the compilation of the TEN
    // runtime (i.e., with -fno-exceptions), it implies that the extensions used
    // will also not employ exceptions (otherwise it would be unreasonable). In
    // this case, the try/catch blocks become no-ops. Conversely, if exceptions
    // are enabled during compilation, then the try/catch here can intercept all
    // exceptions thrown by user code that are not already caught, serving as a
    // kind of fallback.
    try {
      on_configure(ten_env);
    } catch (...) {
      TEN_LOGW("Caught a exception of type '%s' in App on_configure().",
               curr_exception_type_name().c_str());
    }
  }

  static void cpp_app_on_init_cb_wrapper(ten_app_t *app, ::ten_env_t *ten_env) {
    TEN_ASSERT(app, "Should not happen.");
    TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
    TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, true),
               "Should not happen.");
    TEN_ASSERT(ten_app_get_ten_env(app) == ten_env, "Should not happen.");

    auto *cpp_app =
        static_cast<app_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(app)));
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_app->on_init_helper_for_cpp(*cpp_ten_env);
  }

  void on_init_helper_for_cpp(ten_env_t &ten_env) {
    // The TEN runtime does not use C++ exceptions. The use of try/catch here is
    // merely to intercept any exceptions that might be thrown by the user's app
    // code. If exceptions are disabled during the compilation of the TEN
    // runtime (i.e., with -fno-exceptions), it implies that the extensions used
    // will also not employ exceptions (otherwise it would be unreasonable). In
    // this case, the try/catch blocks become no-ops. Conversely, if exceptions
    // are enabled during compilation, then the try/catch here can intercept all
    // exceptions thrown by user code that are not already caught, serving as a
    // kind of fallback.
    try {
      on_init(ten_env);
    } catch (...) {
      TEN_LOGW("Caught a exception of type '%s' in App on_init().",
               curr_exception_type_name().c_str());
    }
  }

  static void cpp_app_on_close_cb_wrapper(ten_app_t *app,
                                          ::ten_env_t *ten_env) {
    TEN_ASSERT(app, "Should not happen.");
    TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
    TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, true),
               "Should not happen.");
    TEN_ASSERT(ten_app_get_ten_env(app) == ten_env, "Should not happen.");

    auto *cpp_app =
        static_cast<app_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(app)));
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));

    cpp_app->on_deinit_helper_for_cpp(*cpp_ten_env);
  }

  void on_deinit_helper_for_cpp(ten_env_t &ten_env) {
    // The TEN runtime does not use C++ exceptions. The use of try/catch here is
    // merely to intercept any exceptions that might be thrown by the user's app
    // code. If exceptions are disabled during the compilation of the TEN
    // runtime (i.e., with -fno-exceptions), it implies that the extensions used
    // will also not employ exceptions (otherwise it would be unreasonable). In
    // this case, the try/catch blocks become no-ops. Conversely, if exceptions
    // are enabled during compilation, then the try/catch here can intercept all
    // exceptions thrown by user code that are not already caught, serving as a
    // kind of fallback.
    try {
      on_deinit(ten_env);
    } catch (...) {
      TEN_LOGW("Caught a exception of type '%s' in App on_close().",
               curr_exception_type_name().c_str());
    }
  }

  ::ten_app_t *c_app = nullptr;
  ten_env_t *cpp_ten_env;
};

}  // namespace ten
