//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/value/value.h"

// Creates a ten_value_t from a JSON string.
// The returned pointer must be freed with ten_value_destroy_proxy.
// If an error occurs, NULL is returned and err_msg is set.
TEN_UTILS_API ten_value_t *ten_value_create_from_json_str_proxy(
    const char *json_str, const char **err_msg);

// Destroys a ten_value_t and frees associated memory.
TEN_UTILS_API void ten_value_destroy_proxy(const ten_value_t *value);
