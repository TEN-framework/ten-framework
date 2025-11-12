//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/fixed_value.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/field/properties.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_convert.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_json.h"
#include "ten_utils/value/value_kv.h"

static void ten_msg_conversion_per_property_rule_fixed_value_init(
    ten_msg_conversion_per_property_rule_fixed_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  self->value = NULL;
}

void ten_msg_conversion_per_property_rule_fixed_value_deinit(
    ten_msg_conversion_per_property_rule_fixed_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->value) {
    ten_value_destroy(self->value);
    self->value = NULL;
  }
}

bool ten_msg_conversion_per_property_rule_fixed_value_convert(
    ten_msg_conversion_per_property_rule_fixed_value_t *self,
    ten_shared_ptr_t *msg, const char *msg_property_path, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(msg_property_path, "Invalid argument.");

  return ten_msg_set_property(msg, msg_property_path,
                              ten_value_clone(self->value), err);
}

bool ten_msg_conversion_per_property_rule_fixed_value_from_json(
    ten_msg_conversion_per_property_rule_fixed_value_t *self, ten_json_t *json,
    TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");

  ten_msg_conversion_per_property_rule_fixed_value_init(self);

  ten_json_t value_json = TEN_JSON_INIT_VAL(json->ctx, false);
  bool success = ten_json_object_peek(json, TEN_STR_VALUE, &value_json);
  TEN_ASSERT(success, "Should not happen.");

  self->value = ten_value_from_json(&value_json);
  if (!self->value) {
    return false;
  }
  return true;
}

bool ten_msg_conversion_per_property_rule_fixed_value_to_json(
    ten_msg_conversion_per_property_rule_fixed_value_t *self, ten_json_t *json,
    TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->value, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self->value), "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");
  TEN_ASSERT(ten_json_check_integrity(json), "Invalid argument.");

  ten_json_t value_json = TEN_JSON_INIT_VAL(json->ctx, false);
  if (!ten_value_to_json(self->value, &value_json)) {
    ten_json_deinit(&value_json);
    return false;
  }

  return ten_json_object_set(json, TEN_STR_VALUE, &value_json);
}

bool ten_msg_conversion_per_property_rule_fixed_value_from_value(
    ten_msg_conversion_per_property_rule_fixed_value_t *self,
    ten_value_t *value, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_msg_conversion_per_property_rule_fixed_value_init(self);

  ten_value_t *fixed_value = ten_value_object_peek(value, TEN_STR_VALUE);
  TEN_ASSERT(fixed_value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(fixed_value), "Should not happen.");

  self->value = ten_value_clone(fixed_value);
  if (!self->value) {
    return false;
  }

  return true;
}

void ten_msg_conversion_per_property_rule_fixed_value_to_value(
    ten_msg_conversion_per_property_rule_fixed_value_t *self,
    ten_value_t *value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");
  TEN_ASSERT(ten_value_is_object(value), "Invalid argument.");

  ten_value_t *result = ten_value_clone(self->value);
  ten_value_kv_t *kv = ten_value_kv_create(TEN_STR_VALUE, result);

  ten_list_push_ptr_back(&value->content.object, kv,
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
}
