//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_env_tester_t ten_env_tester_t;
typedef struct ten_env_tester_proxy_t ten_env_tester_proxy_t;
typedef struct ten_error_t ten_error_t;

typedef void (*ten_env_tester_proxy_notify_func_t)(
    ten_env_tester_t *ten_env_tester, void *user_data);

TEN_RUNTIME_API ten_env_tester_proxy_t *ten_env_tester_proxy_create(
    ten_env_tester_t *ten_env_tester, ten_error_t *err);

TEN_RUNTIME_API bool ten_env_tester_proxy_release(ten_env_tester_proxy_t *self,
                                                  ten_error_t *err);

TEN_RUNTIME_API bool ten_env_tester_proxy_notify(
    ten_env_tester_proxy_t *self,
    ten_env_tester_proxy_notify_func_t notify_func, void *user_data,
    ten_error_t *err);
