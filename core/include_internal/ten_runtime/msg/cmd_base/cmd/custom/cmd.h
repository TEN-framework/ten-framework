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
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/value/value.h"

typedef struct ten_msg_t ten_msg_t;
typedef struct ten_cmd_t ten_cmd_t;

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_custom_set_ten_property(
    ten_msg_t *self, ten_list_t *paths, ten_value_t *value, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_custom_as_msg_destroy(ten_msg_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_t *ten_raw_cmd_custom_as_msg_clone(
    ten_msg_t *self, ten_list_t *excluded_field_ids);

TEN_RUNTIME_API ten_shared_ptr_t *ten_cmd_custom_create_empty(void);

TEN_RUNTIME_PRIVATE_API ten_cmd_t *ten_raw_cmd_custom_create(const char *name,
                                                             ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_cmd_custom_create(
    const char *name, ten_error_t *err);

TEN_RUNTIME_API ten_shared_ptr_t *ten_cmd_custom_create_with_name_len(
    const char *name, size_t name_len, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_custom_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
