//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/value/value.h"

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
typedef struct ten_msg_conversion_per_property_rules_t {
  ten_list_t rules;    // ten_msg_conversion_per_property_rule_t
  bool keep_original;  // Determine whether original properties will be cloned.
} ten_msg_conversion_per_property_rules_t;

TEN_RUNTIME_PRIVATE_API void ten_msg_conversion_per_property_rules_destroy(
    ten_msg_conversion_per_property_rules_t *self);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *
ten_msg_conversion_per_property_rules_convert(
    ten_msg_conversion_per_property_rules_t *self, ten_shared_ptr_t *msg,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *
ten_result_conversion_per_property_rules_convert(
    ten_msg_conversion_per_property_rules_t *self, ten_shared_ptr_t *msg,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_rules_t *
ten_msg_conversion_per_property_rules_from_json(ten_json_t *json,
                                                ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_conversion_per_property_rules_to_json(
    ten_msg_conversion_per_property_rules_t *self, ten_json_t *json,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_per_property_rules_t *
ten_msg_conversion_per_property_rules_from_value(ten_value_t *value,
                                                 ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *
ten_msg_conversion_per_property_rules_to_value(
    ten_msg_conversion_per_property_rules_t *self, ten_error_t *err);
