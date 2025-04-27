//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_utils/lib/signature.h"

#define TEN_GO_VALUE_SIGNATURE 0x34898DCE3ED53FB8U

typedef struct ten_value_t ten_value_t;

typedef struct ten_go_value_t {
  ten_signature_t signature;

  ten_go_bridge_t bridge;

  bool own;
  ten_value_t *c_value;
} ten_go_value_t;

TEN_RUNTIME_PRIVATE_API void ten_go_ten_value_get_type_and_size(
    ten_value_t *self, uint8_t *type, uintptr_t *size);

TEN_RUNTIME_PRIVATE_API void ten_go_ten_value_get_string(
    ten_value_t *self, void *value, ten_go_error_t *status);

TEN_RUNTIME_PRIVATE_API void ten_go_ten_value_get_buf(ten_value_t *self,
                                                      void *value,
                                                      ten_go_error_t *status);

TEN_RUNTIME_PRIVATE_API void ten_go_ten_value_get_ptr(ten_value_t *self,
                                                      ten_go_handle_t *value,
                                                      ten_go_error_t *status);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_go_ten_value_create_buf(void *value,
                                                                 int value_len);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_go_ten_value_create_ptr(
    ten_go_handle_t value);

TEN_RUNTIME_PRIVATE_API bool ten_go_ten_value_to_json(ten_value_t *self,
                                                      uintptr_t *json_str_len,
                                                      const char **json_str,
                                                      ten_go_error_t *status);

TEN_RUNTIME_PRIVATE_API bool ten_go_value_check_integrity(ten_go_value_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_value_go_handle(ten_go_value_t *self);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_go_value_c_value(ten_go_value_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t ten_go_wrap_value(ten_value_t *c_value,
                                                          bool own);
