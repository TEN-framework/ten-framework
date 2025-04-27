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
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
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
    ten_shared_ptr_t *new_msg, const char *new_msg_property_path,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(new_msg && ten_msg_check_integrity(new_msg), "Invalid argument.");
  TEN_ASSERT(new_msg_property_path, "Invalid argument.");

  return ten_msg_set_property(new_msg, new_msg_property_path,
                              ten_value_clone(self->value), err);
}

bool ten_msg_conversion_per_property_rule_fixed_value_from_json(
    ten_msg_conversion_per_property_rule_fixed_value_t *self, ten_json_t *json,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");

  ten_msg_conversion_per_property_rule_fixed_value_init(self);

  ten_json_t value_json = TEN_JSON_INIT_VAL(json->ctx, false);
  bool success = ten_json_object_peek(json, TEN_STR_VALUE, &value_json);
  TEN_ASSERT(success, "Should not happen.");

  TEN_TYPE data_type = ten_json_get_type(&value_json);

  switch (data_type) {
  case TEN_TYPE_INT64:
    self->value =
        ten_value_create_int64(ten_json_get_integer_value(&value_json));
    break;
  case TEN_TYPE_UINT64:
    self->value =
        ten_value_create_uint64(ten_json_get_integer_value(&value_json));
    break;
  case TEN_TYPE_FLOAT64:
    self->value =
        ten_value_create_float64(ten_json_get_real_value(&value_json));
    break;
  case TEN_TYPE_BOOL:
    self->value =
        ten_value_create_bool(ten_json_get_boolean_value(&value_json));
    break;
  case TEN_TYPE_STRING:
    self->value =
        ten_value_create_string(ten_json_peek_string_value(&value_json));
    break;
  default:
    TEN_ASSERT(0, "Handle more types: %d", data_type);
    break;
  }

  if (!self->value) {
    return false;
  }

  return true;
}

bool ten_msg_conversion_per_property_rule_fixed_value_to_json(
    ten_msg_conversion_per_property_rule_fixed_value_t *self, ten_json_t *json,
    ten_error_t *err) {
  TEN_ASSERT(self && self->value && ten_value_check_integrity(self->value),
             "Invalid argument.");
  TEN_ASSERT(json && ten_json_check_integrity(json), "Invalid argument.");

  switch (self->value->type) {
  case TEN_TYPE_INT8:
  case TEN_TYPE_INT16:
  case TEN_TYPE_INT32:
  case TEN_TYPE_INT64:
  case TEN_TYPE_UINT8:
  case TEN_TYPE_UINT16:
  case TEN_TYPE_UINT32:
  case TEN_TYPE_UINT64: {
    int64_t val = ten_value_get_int64(self->value, err);
    if (!ten_error_is_success(err)) {
      return false;
    }
    ten_json_object_set_int(json, TEN_STR_VALUE, val);
    break;
  }

  case TEN_TYPE_FLOAT32:
  case TEN_TYPE_FLOAT64: {
    double val = ten_value_get_float64(self->value, err);
    if (!ten_error_is_success(err)) {
      return false;
    }
    ten_json_object_set_real(json, TEN_STR_VALUE, val);
    break;
  }

  case TEN_TYPE_STRING: {
    const char *value_str = ten_value_peek_raw_str(self->value, err);
    ten_json_object_set_string(json, TEN_STR_VALUE, value_str);
    break;
  }

  case TEN_TYPE_BOOL: {
    bool val = ten_value_get_bool(self->value, err);
    if (!ten_error_is_success(err)) {
      return false;
    }
    ten_json_object_set_bool(json, TEN_STR_VALUE, val);
    break;
  }

  default:
    TEN_ASSERT(0, "Handle more types: %d", self->value->type);
    break;
  }

  return true;
}

bool ten_msg_conversion_per_property_rule_fixed_value_from_value(
    ten_msg_conversion_per_property_rule_fixed_value_t *self,
    ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_msg_conversion_per_property_rule_fixed_value_init(self);

  ten_value_t *fixed_value = ten_value_object_peek(value, TEN_STR_VALUE);
  TEN_ASSERT(fixed_value && ten_value_check_integrity(fixed_value),
             "Should not happen.");

  TEN_TYPE data_type = fixed_value->type;

  switch (data_type) {
  case TEN_TYPE_INT8:
  case TEN_TYPE_INT16:
  case TEN_TYPE_INT32:
  case TEN_TYPE_INT64:
  case TEN_TYPE_UINT8:
  case TEN_TYPE_UINT16:
  case TEN_TYPE_UINT32:
  case TEN_TYPE_UINT64:
  case TEN_TYPE_FLOAT32:
  case TEN_TYPE_FLOAT64:
  case TEN_TYPE_BOOL:
  case TEN_TYPE_STRING:
    self->value = ten_value_clone(fixed_value);
    break;

  default:
    TEN_ASSERT(0, "Handle more types: %d", data_type);
    break;
  }

  if (!self->value) {
    return false;
  }

  return true;
}

void ten_msg_conversion_per_property_rule_fixed_value_to_value(
    ten_msg_conversion_per_property_rule_fixed_value_t *self,
    ten_value_t *value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(value && ten_value_is_object(value), "Invalid argument.");

  ten_value_t *result = ten_value_clone(self->value);
  ten_value_kv_t *kv = ten_value_kv_create(TEN_STR_VALUE, result);

  ten_list_push_ptr_back(&value->content.object, kv,
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
}
