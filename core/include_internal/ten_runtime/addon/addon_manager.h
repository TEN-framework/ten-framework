//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "ten_runtime/addon/addon_manager.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/rwlock.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/ctor.h"

typedef struct ten_app_t ten_app_t;

#define TEN_ADDON_REGISTER(TYPE, NAME, ADDON)                                  \
  static void ____ten_addon_##NAME##_##TYPE##_addon_register_handler__(        \
      ten_addon_registration_t *registration,                                  \
      ten_addon_registration_done_func_t done_callback,                        \
      ten_addon_register_ctx_t *register_ctx, void *user_data) {               \
    TEN_ASSERT(registration, "Invalid argument.");                             \
    TEN_ASSERT(done_callback, "Invalid argument.");                            \
    ten_string_t *base_dir = ten_path_get_module_path(                         \
        (void *)____ten_addon_##NAME##_##TYPE##_addon_register_handler__);     \
    ten_addon_register_##TYPE(#NAME, ten_string_get_raw_str(base_dir),         \
                              (ADDON), register_ctx);                          \
    ten_string_destroy(base_dir);                                              \
    done_callback(register_ctx, user_data);                                    \
  }                                                                            \
  TEN_CONSTRUCTOR(____ctor_ten_declare_##NAME##_##TYPE##_addon____) {          \
    /* Add addon registration function into addon manager. */                  \
    ten_addon_manager_t *manager = ten_addon_manager_get_instance();           \
    bool success = ten_addon_manager_add_addon(                                \
        manager, #TYPE, #NAME,                                                 \
        ____ten_addon_##NAME##_##TYPE##_addon_register_handler__, NULL, NULL); \
    if (!success) {                                                            \
      TEN_LOGE("Failed to register addon: %s", #NAME);                         \
      /* NOLINTNEXTLINE(concurrency-mt-unsafe) */                              \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }

typedef void (*ten_addon_manager_on_all_addons_registered_func_t)(
    void *register_ctx, void *cb_data);

typedef struct ten_addon_manager_t {
  // Define a registry map to store addon registration functions.
  // The key is the addon name (string), and the value is a function that takes
  // a register_ctx.
  ten_list_t registry;  // ten_addon_registration_t*

  // The app that the addon manager belongs to.
  // The addon manager will be destroyed when the app is destroyed.
  ten_app_t *app;

  ten_rwlock_t *rwlock;
} ten_addon_manager_t;

TEN_RUNTIME_PRIVATE_API void ten_addon_manager_destroy(
    ten_addon_manager_t *self);

TEN_RUNTIME_API void ten_addon_manager_register_all_addons(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data);

TEN_RUNTIME_PRIVATE_API void ten_addon_manager_register_all_addon_loaders(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data);

TEN_RUNTIME_PRIVATE_API void ten_addon_manager_register_all_protocols(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data);

TEN_RUNTIME_API bool ten_addon_manager_register_specific_addon(
    ten_addon_manager_t *self, TEN_ADDON_TYPE addon_type,
    const char *addon_name, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data);

TEN_RUNTIME_API bool ten_addon_manager_is_addon_loaded(
    ten_addon_manager_t *self, TEN_ADDON_TYPE addon_type,
    const char *addon_name);

TEN_RUNTIME_PRIVATE_API bool ten_addon_manager_set_belonging_app_if_not_set(
    ten_addon_manager_t *self, ten_app_t *app);

TEN_RUNTIME_PRIVATE_API bool ten_addon_manager_belongs_to_app(
    ten_addon_manager_t *self, ten_app_t *app);

TEN_RUNTIME_PRIVATE_API ten_app_t *ten_addon_manager_get_belonging_app(
    ten_addon_manager_t *self);

TEN_RUNTIME_API ten_addon_register_ctx_t *ten_addon_register_ctx_create(
    ten_app_t *app);

TEN_RUNTIME_API void ten_addon_register_ctx_destroy(
    ten_addon_register_ctx_t *self);
