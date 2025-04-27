//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <cassert>
#include <cstddef>

#include "ten_runtime/addon/addon.h"
#include "ten_runtime/addon/extension/extension.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/cpp/detail/binding_handle.h"
#include "ten_runtime/binding/cpp/detail/common.h"
#include "ten_runtime/binding/cpp/detail/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"

namespace ten {

class addon_t : public binding_handle_t {
 public:
  addon_t()
      : binding_handle_t(ten_addon_create(
            proxy_on_init, proxy_on_deinit, proxy_on_create_instance,
            proxy_on_destroy_instance, proxy_on_destroy)) {
    ten_binding_handle_set_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(get_c_instance()), this);
  }

  ~addon_t() override {
    ten_addon_destroy(static_cast<ten_addon_t *>(get_c_instance()));

    TEN_ASSERT(cpp_ten_env, "Should not happen.");
    delete cpp_ten_env;
  };

  // @{
  addon_t(const addon_t &) = delete;
  addon_t(addon_t &&) = delete;
  addon_t &operator=(const addon_t &) = delete;
  addon_t &operator=(const addon_t &&) = delete;
  // @}

 protected:
  virtual void on_init(ten_env_t &ten_env) { ten_env.on_init_done(); }

  virtual void on_deinit(ten_env_t &ten_env) { ten_env.on_deinit_done(); }

  virtual void on_create_instance(ten_env_t &ten_env, const char *name,
                                  void *context) {
    (void)ten_env;
    (void)name;
    (void)context;

    // If a subclass requires the functionality of this function, it needs to
    // override this function.
    TEN_ASSERT(0, "Should not happen.");
  };

  virtual void on_destroy_instance(ten_env_t &ten_env, void *instance,
                                   void *context) {
    (void)ten_env;
    (void)instance;
    (void)context;

    // If a subclass requires the functionality of this function, it needs to
    // override this function.
    TEN_ASSERT(0, "Should not happen.");
  };

 private:
  ten_env_t *cpp_ten_env{};

  void invoke_cpp_addon_on_init(ten_env_t &ten_env) {
    try {
      on_init(ten_env);
    } catch (...) {
      TEN_LOGW("Caught a exception of type '%s' in addon on_init().",
               curr_exception_type_name().c_str());
    }
  }

  void invoke_cpp_addon_on_deinit(ten_env_t &ten_env) {
    try {
      on_deinit(ten_env);
    } catch (...) {
      TEN_LOGW("Caught a exception '%s' in addon on_deinit().",
               curr_exception_type_name().c_str());
    }
  }

  void invoke_cpp_addon_on_create_instance(ten_env_t &ten_env, const char *name,
                                           void *context) {
    try {
      on_create_instance(ten_env, name, context);
    } catch (...) {
      TEN_LOGW("Caught a exception '%s' in addon on_create_instance(%s).",
               curr_exception_type_name().c_str(), name);
    }
  }

  void invoke_cpp_addon_on_destroy_instance(ten_env_t &ten_env, void *instance,
                                            void *context) {
    try {
      on_destroy_instance(ten_env, instance, context);
    } catch (...) {
      TEN_LOGW("Caught a exception '%s' in addon on_destroy_instance().",
               curr_exception_type_name().c_str());
    }
  }

  static void proxy_on_init(ten_addon_t *addon, ::ten_env_t *ten_env) {
    TEN_ASSERT(addon && ten_env, "Invalid argument.");

    auto *cpp_addon =
        static_cast<addon_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon)));
    TEN_ASSERT(!ten_binding_handle_get_me_in_target_lang(
                   reinterpret_cast<ten_binding_handle_t *>(ten_env)),
               "Should not happen.");

    auto *cpp_ten_env = new ten_env_t(ten_env);
    TEN_ASSERT(cpp_addon && cpp_ten_env, "Should not happen.");

    // Remember it so that we can destroy it when C++ addon is destroyed.
    cpp_addon->cpp_ten_env = cpp_ten_env;

    cpp_addon->invoke_cpp_addon_on_init(*cpp_ten_env);
  }

  static void proxy_on_deinit(ten_addon_t *addon, ::ten_env_t *ten_env) {
    TEN_ASSERT(addon && ten_env, "Should not happen.");

    auto *cpp_addon =
        static_cast<addon_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon)));
    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));
    TEN_ASSERT(cpp_addon && cpp_ten_env, "Should not happen.");

    cpp_addon->invoke_cpp_addon_on_deinit(*cpp_ten_env);
  }

  static void proxy_on_create_instance(ten_addon_t *addon, ::ten_env_t *ten_env,
                                       const char *name, void *context) {
    TEN_ASSERT(addon && ten_env && name && strlen(name), "Invalid argument.");

    auto *cpp_addon =
        static_cast<addon_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon)));
    TEN_ASSERT(cpp_addon, "Should not happen.");

    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));
    TEN_ASSERT(cpp_ten_env, "Should not happen.");

    cpp_addon->invoke_cpp_addon_on_create_instance(*cpp_ten_env, name, context);
  }

  static void proxy_on_destroy_instance(ten_addon_t *addon,
                                        ::ten_env_t *ten_env, void *instance,
                                        void *context) {
    TEN_ASSERT(addon && ten_env && instance, "Invalid argument.");

    auto *cpp_addon =
        static_cast<addon_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon)));

    auto *cpp_ten_env =
        static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(ten_env)));
    TEN_ASSERT(cpp_ten_env, "Should not happen.");

    auto *cpp_instance = ten_binding_handle_get_me_in_target_lang(
        static_cast<ten_binding_handle_t *>(instance));
    TEN_ASSERT(cpp_instance, "Should not happen.");

    cpp_addon->invoke_cpp_addon_on_destroy_instance(*cpp_ten_env, cpp_instance,
                                                    context);
  }

  static void proxy_on_destroy(ten_addon_t *addon) {
    TEN_ASSERT(addon, "Invalid argument.");

    auto *cpp_addon =
        static_cast<addon_t *>(ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(addon)));

    delete cpp_addon;
  }
};

}  // namespace ten
