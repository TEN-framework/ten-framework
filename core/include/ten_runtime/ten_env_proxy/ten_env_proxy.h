//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"

typedef struct ten_env_proxy_t ten_env_proxy_t;
typedef struct ten_env_t ten_env_t;

typedef void (*ten_env_proxy_notify_func_t)(ten_env_t *ten_env,
                                            void *user_data);

TEN_RUNTIME_API ten_env_proxy_t *ten_env_proxy_create(ten_env_t *ten_env,
                                                      size_t initial_thread_cnt,
                                                      ten_error_t *err);

TEN_RUNTIME_API bool ten_env_proxy_release(ten_env_proxy_t *self,
                                           ten_error_t *err);

TEN_RUNTIME_API bool ten_env_proxy_notify(
    ten_env_proxy_t *self, ten_env_proxy_notify_func_t notify_func,
    void *user_data, bool sync, ten_error_t *err);

TEN_RUNTIME_API bool ten_env_proxy_acquire_lock_mode(ten_env_proxy_t *self,
                                                     ten_error_t *err);

TEN_RUNTIME_API bool ten_env_proxy_release_lock_mode(ten_env_proxy_t *self,
                                                     ten_error_t *err);
