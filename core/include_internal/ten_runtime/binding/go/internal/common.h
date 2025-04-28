//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>

#include "src/ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_utils/lib/error.h"

#define TEN_GO_STATUS_ERR_MSG_BUF_SIZE 256

TEN_RUNTIME_PRIVATE_API ten_go_handle_array_t *ten_go_handle_array_create(
    size_t size);

TEN_RUNTIME_PRIVATE_API void ten_go_handle_array_destroy(
    ten_go_handle_array_t *self);

TEN_RUNTIME_PRIVATE_API char *ten_go_str_dup(const char *str);

TEN_RUNTIME_PRIVATE_API void ten_go_bridge_destroy_c_part(
    ten_go_bridge_t *self);

TEN_RUNTIME_PRIVATE_API void ten_go_bridge_destroy_go_part(
    ten_go_bridge_t *self);

TEN_RUNTIME_PRIVATE_API void ten_go_error_init_with_error_code(
    ten_go_error_t *self, ten_error_code_t error_code);

TEN_RUNTIME_PRIVATE_API void ten_go_error_from_error(ten_go_error_t *self,
                                                     ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_go_error_set_error_code(
    ten_go_error_t *self, ten_error_code_t error_code);

TEN_RUNTIME_PRIVATE_API void ten_go_error_set(ten_go_error_t *self,
                                              ten_error_code_t error_code,
                                              const char *error_message);
