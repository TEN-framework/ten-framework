//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg_conversion/msg_conversion/base.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/value/value.h"

typedef struct ten_msg_conversion_per_property_rules_t
    ten_msg_conversion_per_property_rules_t;

// {
//   "type": "per_property",
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
typedef struct ten_msg_conversion_per_property_t {
  ten_msg_conversion_t base;
  ten_msg_conversion_per_property_rules_t *rules;
} ten_msg_conversion_per_property_t;

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_t *
ten_msg_conversion_per_property_create(
    ten_msg_conversion_per_property_rules_t *rules);

TEN_RUNTIME_PRIVATE_API void ten_msg_conversion_per_property_destroy(
    ten_msg_conversion_per_property_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_t *
ten_msg_conversion_per_property_from_json(ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_conversion_per_property_to_json(
    ten_msg_conversion_per_property_t *self, ten_json_t *json,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_t *
ten_msg_conversion_per_property_from_value(ten_value_t *value,
                                           ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_msg_conversion_per_property_to_value(
    ten_msg_conversion_per_property_t *self, ten_error_t *err);
