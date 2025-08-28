//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/addon/addon.h"

typedef struct ten_addon_manager_t ten_addon_manager_t;
typedef struct ten_error_t ten_error_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_addon_register_ctx_t ten_addon_register_ctx_t;

// Forward declarations.
typedef struct ten_addon_registration_t ten_addon_registration_t;

typedef void (*ten_addon_registration_done_func_t)(void *register_ctx,
                                                   void *user_data);

typedef void (*ten_addon_registration_func_t)(
    ten_addon_registration_t *registration,
    ten_addon_registration_done_func_t done_callback,
    ten_addon_register_ctx_t *register_ctx, void *user_data);

typedef struct ten_addon_registration_t {
  TEN_ADDON_TYPE addon_type;
  ten_string_t addon_name;
  ten_addon_registration_func_t func;
  void *context;
} ten_addon_registration_t;

typedef struct ten_addon_register_ctx_t {
  ten_app_t *app;
} ten_addon_register_ctx_t;

TEN_RUNTIME_API ten_addon_manager_t *ten_addon_manager_get_instance(void);

TEN_RUNTIME_API bool ten_addon_manager_add_addon(
    ten_addon_manager_t *self, const char *addon_type_str,
    const char *addon_name, ten_addon_registration_func_t func, void *context,
    ten_error_t *error);
