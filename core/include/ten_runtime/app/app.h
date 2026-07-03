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

typedef struct ten_app_t ten_app_t;
typedef struct ten_metadata_info_t ten_metadata_info_t;
typedef struct ten_env_t ten_env_t;

typedef void (*ten_app_on_configure_func_t)(ten_app_t *app, ten_env_t *ten_env);

typedef void (*ten_app_on_init_func_t)(ten_app_t *app, ten_env_t *ten_env);

typedef void (*ten_app_on_deinit_func_t)(ten_app_t *app, ten_env_t *ten_env);

TEN_RUNTIME_API ten_app_t *ten_app_create(
    ten_app_on_configure_func_t on_configure, ten_app_on_init_func_t on_init,
    ten_app_on_deinit_func_t on_deinit, ten_error_t *err);

TEN_RUNTIME_API void ten_app_destroy(ten_app_t *self);

TEN_RUNTIME_API bool ten_app_close(ten_app_t *self, ten_error_t *err);

TEN_RUNTIME_API bool ten_app_run(ten_app_t *self, bool run_in_background,
                                 ten_error_t *err);

TEN_RUNTIME_API bool ten_app_wait(ten_app_t *self, ten_error_t *err);

TEN_RUNTIME_API bool ten_app_check_integrity(ten_app_t *self,
                                             bool check_thread);

TEN_RUNTIME_API ten_env_t *ten_app_get_ten_env(ten_app_t *self);
