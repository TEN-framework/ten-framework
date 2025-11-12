//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/error.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_extension_group_create_extensions_done_ctx_t
    ten_extension_group_create_extensions_done_ctx_t;

TEN_RUNTIME_PRIVATE_API bool ten_env_on_create_extensions_done(
    ten_env_t *self, ten_extension_group_create_extensions_done_ctx_t *ctx,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_env_on_destroy_extensions_done(
    ten_env_t *self, ten_error_t *err);
