//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/fixed_value.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/from_original.h"
#include "ten_utils/lib/error.h"

/**
 * TEN runtime only provides basic message conversion methods, with plans
 * for moderate expansion in the future. If you need more flexible message
 * conversion, you can develop your own extension for message conversion. In
 * this extension's on_cmd/data/... APIs, you can perform any kind of message
 * conversion.
 */
typedef enum TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE {
  TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_INVALID,

  TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL,
  TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE,
} TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE;

// {
//   "rules": [{
//     "path": "...",
//     "conversion_mode": "from_original",
//     "original_path": "..."
//   },{
//     "path": "...",
//     "conversion_mode": "fixed_value",
//     "value": "..."
//   }]
// }
typedef struct ten_msg_conversion_per_property_rule_t {
  ten_string_t property_path;

  TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE conversion_mode;

  union {
    ten_msg_conversion_per_property_rule_from_original_t from_original;
    ten_msg_conversion_per_property_rule_fixed_value_t fixed_value;
  } u;
} ten_msg_conversion_per_property_rule_t;

TEN_RUNTIME_PRIVATE_API void ten_msg_conversion_per_property_rule_destroy(
    ten_msg_conversion_per_property_rule_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_msg_conversion_per_property_rule_convert(
    ten_msg_conversion_per_property_rule_t *self, ten_shared_ptr_t *msg,
    ten_shared_ptr_t *new_msg, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_rule_t *
ten_msg_conversion_per_property_rule_from_json(ten_json_t *json,
                                               ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_conversion_per_property_rule_to_json(
    ten_msg_conversion_per_property_rule_t *self, ten_json_t *json,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_rule_t *
ten_msg_conversion_per_property_rule_from_value(ten_value_t *value,
                                                ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *
ten_msg_conversion_per_property_rule_to_value(
    ten_msg_conversion_per_property_rule_t *self, ten_error_t *err);
