//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

// This header file will be used by the Rust `bindgen` tool to generate the
// FFIs.

// In Rust, `ten_schema_t` will be represented as a raw pointer to an opaque
// struct. Rust cannot directly access the fields of a raw pointer, even if it
// knows the structure's layout.
typedef struct ten_schema_t ten_schema_t;

TEN_UTILS_API ten_schema_t *ten_schema_create_from_json_str_proxy(
    const char *json_str, const char **err_msg);

TEN_UTILS_API void ten_schema_destroy_proxy(const ten_schema_t *self);

TEN_UTILS_API bool ten_schema_adjust_and_validate_json_str_proxy(
    const ten_schema_t *self, const char *json_str, const char **err_msg);

TEN_UTILS_API bool ten_schema_is_compatible_proxy(const ten_schema_t *self,
                                                  const ten_schema_t *target,
                                                  const char **err_msg);
