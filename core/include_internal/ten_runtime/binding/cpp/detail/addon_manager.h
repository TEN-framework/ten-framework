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

#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"  // IWYU pragma: keep
#include "ten_runtime/addon/addon_manager.h"       // IWYU pragma: keep
#include "ten_runtime/binding/cpp/detail/addon.h"  // IWYU pragma: keep

#define TEN_CPP_REGISTER_ADDON_AS_ADDON_LOADER(NAME, CLASS)                      \
  class NAME##_default_addon_loader_addon_t : public ten::addon_t {              \
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
  static void ____ten_addon_##NAME##_register_handler__(                         \
      TEN_UNUSED TEN_ADDON_TYPE addon_type,                                      \
      TEN_UNUSED ten_string_t *addon_name, void *register_ctx,                   \
      TEN_UNUSED void *user_data) {                                              \
    auto *addon_instance = new NAME##_default_addon_loader_addon_t();            \
    ten_string_t *base_dir =                                                     \
        ten_path_get_module_path(/* NOLINTNEXTLINE */                            \
                                 (void *)                                        \
                                     ____ten_addon_##NAME##_register_handler__); \
    ten_addon_register_addon_loader(                                             \
        #NAME, ten_string_get_raw_str(base_dir),                                 \
        static_cast<ten_addon_t *>(addon_instance->get_c_instance()),            \
        register_ctx);                                                           \
    ten_string_destroy(base_dir);                                                \
  }                                                                              \
  TEN_CONSTRUCTOR(____ten_addon_##NAME##_registrar____) {                        \
    /* Add addon registration function into addon manager. */                    \
    ten_addon_manager_t *manager = ten_addon_manager_get_instance();             \
    bool success = ten_addon_manager_add_addon(                                  \
        manager, "addon_loader", #NAME,                                          \
        ____ten_addon_##NAME##_register_handler__, NULL, NULL);                  \
    if (!success) {                                                              \
      TEN_LOGF("Failed to register addon: %s", #NAME);                           \
      /* NOLINTNEXTLINE(concurrency-mt-unsafe) */                                \
      exit(EXIT_FAILURE);                                                        \
    }                                                                            \
  }
