//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "ten_runtime/ten_env/ten_env.h"

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_init(ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_deinit(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_addon_create_extension_done(
    ten_env_t *self, void *instance, ten_addon_context_t *addon_context);

TEN_RUNTIME_PRIVATE_API void
ten_extension_group_on_addon_destroy_extension_done(
    ten_env_t *self, ten_addon_context_t *addon_context);

TEN_RUNTIME_PRIVATE_API bool ten_extension_group_on_ten_env_proxy_released(
    ten_env_t *self);
