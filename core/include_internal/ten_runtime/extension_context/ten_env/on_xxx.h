//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"

typedef struct ten_env_t ten_env_t;

typedef struct
    ten_extension_context_on_addon_create_extension_group_done_ctx_t {
  ten_extension_group_t *extension_group;
  ten_addon_context_t *addon_context;
} ten_extension_context_on_addon_create_extension_group_done_ctx_t;

TEN_RUNTIME_PRIVATE_API ten_extension_context_on_addon_create_extension_group_done_ctx_t *
ten_extension_context_on_addon_create_extension_group_done_ctx_create(void);

TEN_RUNTIME_PRIVATE_API void
ten_extension_context_on_addon_create_extension_group_done_ctx_destroy(
    ten_extension_context_on_addon_create_extension_group_done_ctx_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_extension_context_on_addon_create_extension_group_done(
    ten_env_t *self, void *instance, ten_addon_context_t *addon_context);
