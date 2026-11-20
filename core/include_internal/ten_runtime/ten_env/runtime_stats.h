//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"

TEN_RUNTIME_PRIVATE_API bool ten_env_is_runtime_stats_property(
    const char *path);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_env_peek_runtime_stats(
    ten_env_t *self, const char *path, ten_error_t *err);
