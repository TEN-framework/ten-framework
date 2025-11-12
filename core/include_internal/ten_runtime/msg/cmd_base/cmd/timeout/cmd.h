//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"

typedef struct ten_cmd_timeout_t {
  ten_cmd_t cmd_hdr;

  ten_value_t timer_id;  // uint32
} ten_cmd_timeout_t;

TEN_RUNTIME_PRIVATE_API void ten_cmd_timeout_set_timer_id(
    ten_shared_ptr_t *self, uint32_t timer_id);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_cmd_timeout_create(
    uint32_t timer_id);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_timeout_as_msg_destroy(
    ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_timeout_set_timer_id(
    ten_cmd_timeout_t *self, uint32_t timer_id);

TEN_RUNTIME_PRIVATE_API uint32_t
ten_raw_cmd_timeout_get_timer_id(ten_cmd_timeout_t *self);

TEN_RUNTIME_API uint32_t ten_cmd_timeout_get_timer_id(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_timeout_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
