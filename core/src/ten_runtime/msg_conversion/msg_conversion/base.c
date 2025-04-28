//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/base.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/per_property.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

void ten_msg_conversion_destroy(ten_msg_conversion_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  switch (self->type) {
  case TEN_MSG_CONVERSION_TYPE_PER_PROPERTY:
    ten_msg_conversion_per_property_destroy(
        (ten_msg_conversion_per_property_t *)self);
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

ten_msg_conversion_t *ten_msg_conversion_from_json(ten_json_t *json,
                                                   ten_error_t *err) {
  TEN_ASSERT(json, "Invalid argument.");

  const char *type = ten_json_object_peek_string(json, TEN_STR_TYPE);

  if (ten_c_string_is_equal(type, TEN_STR_PER_PROPERTY)) {
    return (ten_msg_conversion_t *)ten_msg_conversion_per_property_from_json(
        json, err);
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                    "Invalid message conversion operation type %s", type);
    }
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

bool ten_msg_conversion_to_json(ten_msg_conversion_t *self, ten_json_t *json,
                                ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");

  switch (self->type) {
  case TEN_MSG_CONVERSION_TYPE_PER_PROPERTY:
    return ten_msg_conversion_per_property_to_json(
        (ten_msg_conversion_per_property_t *)self, json, err);
  default:
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
}

ten_msg_conversion_t *ten_msg_conversion_from_value(ten_value_t *value,
                                                    ten_error_t *err) {
  ten_value_t *type_value = ten_value_object_peek(value, TEN_STR_TYPE);
  if (!type_value) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                    "operation_type is missing.");
    }
    return NULL;
  }

  if (!ten_value_is_string(type_value)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                    "operation_type is not a string.");
    }
    return NULL;
  }

  const char *type_str = ten_value_peek_raw_str(type_value, err);
  if (!type_str) {
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  if (!strcmp(type_str, TEN_STR_PER_PROPERTY)) {
    return (ten_msg_conversion_t *)ten_msg_conversion_per_property_from_value(
        value, err);
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                    "Unsupported operation type %s", type_str);
    }
    return NULL;
  }
}

ten_value_t *ten_msg_conversion_to_value(ten_msg_conversion_t *self,
                                         ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  switch (self->type) {
  case TEN_MSG_CONVERSION_TYPE_PER_PROPERTY:
    return ten_msg_conversion_per_property_to_value(
        (ten_msg_conversion_per_property_t *)self, err);
  default:
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

ten_shared_ptr_t *ten_msg_conversion_convert(ten_msg_conversion_t *self,
                                             ten_shared_ptr_t *msg,
                                             ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  return self->operation(self, msg, err);
}
