//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/common/error_code.h"             // IWYU pragma: keep
#include "ten_runtime/ten_env/internal/log.h"          // IWYU pragma: keep
#include "ten_runtime/ten_env/internal/metadata.h"     // IWYU pragma: keep
#include "ten_runtime/ten_env/internal/on_xxx_done.h"  // IWYU pragma: keep
#include "ten_runtime/ten_env/internal/return.h"       // IWYU pragma: keep
#include "ten_runtime/ten_env/internal/send.h"         // IWYU pragma: keep

typedef struct ten_env_t ten_env_t;
typedef struct ten_extension_group_t ten_extension_group_t;

TEN_RUNTIME_API bool ten_env_check_integrity(ten_env_t *self,
                                             bool check_thread);

TEN_RUNTIME_API void *ten_env_get_attached_target(ten_env_t *self);

TEN_RUNTIME_API void ten_env_destroy(ten_env_t *self);
