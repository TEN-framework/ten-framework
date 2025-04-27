//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"

TEN_RUNTIME_PRIVATE_API bool ten_go_ten_env_check_integrity(
    ten_go_ten_env_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_ten_env_t *ten_go_ten_env_reinterpret(
    uintptr_t bridge_addr);

TEN_RUNTIME_PRIVATE_API ten_go_ten_env_t *ten_go_ten_env_wrap(
    ten_env_t *c_ten_env);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_ten_env_go_handle(ten_go_ten_env_t *self);

extern void tenGoCAsyncApiCallback(ten_go_handle_t callback,
                                   ten_go_error_t cgo_error);
