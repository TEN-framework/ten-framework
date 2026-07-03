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
#include "ten_runtime/ten_env/ten_env.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_addon_store_t ten_addon_store_t;
typedef struct ten_addon_t ten_addon_t;
typedef struct ten_addon_host_t ten_addon_host_t;

TEN_RUNTIME_PRIVATE_API bool ten_addon_create_extension_group(
    ten_env_t *ten_env, const char *addon_name, const char *instance_name,
    ten_env_addon_create_instance_done_cb_t cb, void *user_data);

TEN_RUNTIME_PRIVATE_API bool ten_addon_destroy_extension_group(
    ten_env_t *ten_env, ten_extension_group_t *extension_group,
    ten_env_addon_destroy_instance_done_cb_t cb, void *user_data);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_addon_register_extension_group(
    const char *name, const char *base_dir, ten_addon_t *addon,
    void *register_ctx);

TEN_RUNTIME_PRIVATE_API void ten_addon_unregister_all_extension_group(
    ten_env_t *ten_env);
