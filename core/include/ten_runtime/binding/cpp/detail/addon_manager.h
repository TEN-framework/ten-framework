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

#include "ten_runtime/addon/addon_manager.h"  // IWYU pragma: export
#include "ten_utils/macro/ctor.h"

#define TEN_CPP_REGISTER_ADDON_AS_EXTENSION(NAME, CLASS)                         \
  class NAME##_default_extension_addon_t : public ten::addon_t {                 \
   public:                                                                       \
    void on_create_instance(ten::ten_env_t &ten_env, const char *name,           \
                            void *context) override {                            \
      auto *instance = new CLASS(name);                                          \
      ten_env.on_create_instance_done(instance, context);                        \
    }                                                                            \
    void on_destroy_instance(ten::ten_env_t &ten_env, void *instance,            \
                             void *context) override {                           \
      delete static_cast<CLASS *>(instance);                                     \
      ten_env.on_destroy_instance_done(context);                                 \
    }                                                                            \
  };                                                                             \
  namespace {                                                                    \
  void ____ten_addon_##NAME##_register_handler__(                                \
      ten_addon_registration_t *registration,                                    \
      ten_addon_registration_done_func_t done_callback,                          \
      ten_addon_register_ctx_t *register_ctx, void *user_data) {                 \
    if (ten_addon_is_registered(register_ctx, TEN_ADDON_TYPE_EXTENSION,          \
                                #NAME)) {                                        \
      TEN_LOGW("Addon '%s' is already registered", #NAME);                       \
      done_callback(register_ctx, user_data);                                    \
      return;                                                                    \
    }                                                                            \
    auto *addon_instance = new NAME##_default_extension_addon_t();               \
    ten_string_t *base_dir =                                                     \
        ten_path_get_module_path(/* NOLINTNEXTLINE */                            \
                                 (void *)                                        \
                                     ____ten_addon_##NAME##_register_handler__); \
    TEN_ASSERT(registration, "Invalid argument.");                               \
    TEN_ASSERT(done_callback, "Invalid argument.");                              \
    ten_addon_register_extension(                                                \
        #NAME, ten_string_get_raw_str(base_dir),                                 \
        static_cast<ten_addon_t *>(addon_instance->get_c_instance()),            \
        register_ctx);                                                           \
    ten_string_destroy(base_dir);                                                \
    done_callback(register_ctx, user_data);                                      \
  }                                                                              \
  TEN_CONSTRUCTOR(____ten_addon_##NAME##_registrar____) {                        \
    /* Add addon registration function into addon manager. */                    \
    ten_addon_manager_t *manager = ten_addon_manager_get_instance();             \
    bool success = ten_addon_manager_add_addon(                                  \
        manager, "extension", #NAME,                                             \
        ____ten_addon_##NAME##_register_handler__, NULL, NULL);                  \
    if (!success) {                                                              \
      TEN_LOGE("Failed to register addon: %s", #NAME);                           \
      /* NOLINTNEXTLINE(concurrency-mt-unsafe) */                                \
      exit(EXIT_FAILURE);                                                        \
    }                                                                            \
  }                                                                              \
  }
