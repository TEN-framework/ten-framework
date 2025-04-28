//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/from_original.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/field/properties.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

static void ten_msg_conversion_per_property_rule_from_original_init(
    ten_msg_conversion_per_property_rule_from_original_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_STRING_INIT(self->original_path);
}

void ten_msg_conversion_per_property_rule_from_original_deinit(
    ten_msg_conversion_per_property_rule_from_original_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->original_path);
}

static ten_value_t *
ten_msg_conversion_per_property_rule_from_original_get_value(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  return ten_msg_peek_property(
      msg, ten_string_get_raw_str(&self->original_path), NULL);
}

bool ten_msg_conversion_per_property_rule_from_original_convert(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_shared_ptr_t *msg, ten_shared_ptr_t *new_msg,
    const char *new_msg_property_path, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(new_msg && ten_msg_check_integrity(new_msg), "Invalid argument.");
  TEN_ASSERT(new_msg_property_path, "Invalid argument.");

  ten_value_t *msg_property =
      ten_msg_conversion_per_property_rule_from_original_get_value(self, msg);

  return ten_msg_set_property(
      new_msg, new_msg_property_path,
      msg_property ? ten_value_clone(msg_property) : ten_value_create_invalid(),
      err);
}

void ten_msg_conversion_per_property_rule_from_original_from_json(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_json_t *json) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");

  ten_msg_conversion_per_property_rule_from_original_init(self);

  ten_json_t path_json = TEN_JSON_INIT_VAL(json->ctx, false);
  bool success = ten_json_object_peek(json, TEN_STR_ORIGINAL_PATH, &path_json);
  TEN_ASSERT(success, "Should not happen.");

  ten_string_set_formatted(&self->original_path,
                           ten_json_peek_string_value(&path_json));
}

bool ten_msg_conversion_per_property_rule_from_original_to_json(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_json_t *json, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(json && ten_json_check_integrity(json), "Invalid argument.");

  ten_json_object_set_string(json, TEN_STR_ORIGINAL_PATH,
                             ten_string_get_raw_str(&self->original_path));

  return true;
}

bool ten_msg_conversion_per_property_rule_from_original_from_value(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_msg_conversion_per_property_rule_from_original_init(self);
  ten_string_set_formatted(
      &self->original_path,
      ten_value_peek_raw_str(
          ten_value_object_peek(value, TEN_STR_ORIGINAL_PATH), err));

  return true;
}

void ten_msg_conversion_per_property_rule_from_original_to_value(
    ten_msg_conversion_per_property_rule_from_original_t *self,
    ten_value_t *value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value && ten_value_is_object(value), "Invalid argument.");

  ten_value_t *result =
      ten_value_create_string(ten_string_get_raw_str(&self->original_path));
  ten_value_kv_t *kv = ten_value_kv_create(TEN_STR_ORIGINAL_PATH, result);

  ten_list_push_ptr_back(&value->content.object, kv,
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
}
