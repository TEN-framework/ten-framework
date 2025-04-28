//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "ten_utils/lib/error.h"

typedef struct ten_app_t ten_app_t;

typedef void (*ten_addon_register_func_t)(void *register_ctx);

typedef void (
    *ten_addon_try_load_specific_addon_using_all_addon_loaders_func_t)(
    ten_env_t *ten_env, void *cb_data);

typedef void (*ten_addon_load_all_extensions_from_app_base_dir_func_t)(
    ten_env_t *ten_env, void *cb_data);

TEN_RUNTIME_PRIVATE_API bool
ten_addon_load_all_protocols_and_addon_loaders_from_app_base_dir(
    ten_app_t *app, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool
ten_addon_try_load_specific_addon_using_native_addon_loader(
    const char *app_base_dir, TEN_ADDON_TYPE addon_type, const char *addon_name,
    void *register_ctx, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void
ten_addon_try_load_specific_addon_using_all_addon_loaders(
    ten_env_t *ten_env, TEN_ADDON_TYPE addon_type, const char *addon_name,
    ten_addon_try_load_specific_addon_using_all_addon_loaders_func_t cb,
    void *cb_data);

// Traverse the <app_base_dir>/ten_packages/extension/ directory and attempt
// to load all extension addons using all available addon_loaders. After this
// method completes, all addons in the directory are expected to be loaded.
// However, it only performs loading (phase 1) without executing registration
// (phase 2). This means all register functions are added to the addon_manager's
// registry but not executed.
TEN_RUNTIME_PRIVATE_API bool ten_addon_load_all_extensions_from_app_base_dir(
    ten_env_t *ten_env, const char *app_base_dir,
    ten_addon_load_all_extensions_from_app_base_dir_func_t cb, void *cb_data,
    ten_error_t *err);
