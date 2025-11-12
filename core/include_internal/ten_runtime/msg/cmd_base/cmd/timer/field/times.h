//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/msg/loop_fields.h"

typedef struct ten_msg_t ten_msg_t;
typedef struct ten_error_t ten_error_t;

TEN_RUNTIME_PRIVATE_API bool ten_cmd_timer_process_times(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
