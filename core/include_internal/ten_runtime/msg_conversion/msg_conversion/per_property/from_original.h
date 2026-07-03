//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"

// {
//   "path": "...",
//   "conversion_mode": "from_original",
//   "original_path": "..."
// }
typedef struct ten_msg_conversion_per_property_rule_from_original_t {
  ten_string_t original_path;
} ten_msg_conversion_per_property_rule_from_original_t;

TEN_RUNTIME_PRIVATE_API void
ten_msg_conversion_per_property_rule_from_original_deinit(
    ten_msg_conversion_per_property_rule_from_original_t *self);

TEN_RUNTIME_PRIVATE_API bool
ten_msg_conversion_per_property_rule_from_original_convert(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_shared_ptr_t *msg, ten_shared_ptr_t *new_msg,
    const char *new_msg_property_path, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void
ten_msg_conversion_per_property_rule_from_original_from_json(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_json_t *json);

TEN_RUNTIME_PRIVATE_API bool
ten_msg_conversion_per_property_rule_from_original_to_json(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool
ten_msg_conversion_per_property_rule_from_original_from_value(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_value_t *value, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void
ten_msg_conversion_per_property_rule_from_original_to_value(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_value_t *value);
