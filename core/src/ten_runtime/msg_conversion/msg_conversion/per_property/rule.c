//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/rule.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/fixed_value.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

static void ten_msg_conversion_per_property_rule_init(
    ten_msg_conversion_per_property_rule_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_STRING_INIT(self->property_path);
  self->conversion_mode =
      TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_INVALID;
}

static void ten_msg_conversion_per_property_rule_deinit(
    ten_msg_conversion_per_property_rule_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->property_path);

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    ten_msg_conversion_per_property_rule_from_original_deinit(
        &self->u.from_original);
    break;
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    ten_msg_conversion_per_property_rule_fixed_value_deinit(
        &self->u.fixed_value);
    break;
  default:
    break;
  }

  self->conversion_mode =
      TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_INVALID;
}

static ten_msg_conversion_per_property_rule_t *
ten_msg_conversion_per_property_rule_create(void) {
  ten_msg_conversion_per_property_rule_t *self =
      (ten_msg_conversion_per_property_rule_t *)TEN_MALLOC(
          sizeof(ten_msg_conversion_per_property_rule_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_msg_conversion_per_property_rule_init(self);

  return self;
}

void ten_msg_conversion_per_property_rule_destroy(
    ten_msg_conversion_per_property_rule_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_msg_conversion_per_property_rule_deinit(self);
  TEN_FREE(self);
}

bool ten_msg_conversion_per_property_rule_convert(
    ten_msg_conversion_per_property_rule_t *self, ten_shared_ptr_t *msg,
    ten_shared_ptr_t *new_msg, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(new_msg && ten_msg_check_integrity(new_msg), "Invalid argument.");

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    return ten_msg_conversion_per_property_rule_from_original_convert(
        &self->u.from_original, msg, new_msg,
        ten_string_get_raw_str(&self->property_path), err);

  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    return ten_msg_conversion_per_property_rule_fixed_value_convert(
        &self->u.fixed_value, new_msg,
        ten_string_get_raw_str(&self->property_path), err);

  default:
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
}

static TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE
ten_msg_conversion_per_property_rule_conversion_mode_from_string(
    const char *conversion_mode_str, ten_error_t *err) {
  if (ten_c_string_is_equal(conversion_mode_str, TEN_STR_FIXED_VALUE)) {
    return TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE;
  } else if (ten_c_string_is_equal(conversion_mode_str,
                                   TEN_STR_FROM_ORIGINAL)) {
    return TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL;
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Unsupported conversion mode '%s'", conversion_mode_str);
    }
    TEN_ASSERT(0, "Should not happen.");
    return TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_INVALID;
  }
}

ten_msg_conversion_per_property_rule_t *
ten_msg_conversion_per_property_rule_from_json(ten_json_t *json,
                                               ten_error_t *err) {
  TEN_ASSERT(json, "Invalid argument.");

  ten_msg_conversion_per_property_rule_t *self =
      ten_msg_conversion_per_property_rule_create();

  ten_string_init_formatted(&self->property_path, "%s",
                            ten_json_object_peek_string(json, TEN_STR_PATH));

  const char *conversion_mode_str =
      ten_json_object_peek_string(json, TEN_STR_CONVERSION_MODE);

  self->conversion_mode =
      ten_msg_conversion_per_property_rule_conversion_mode_from_string(
          conversion_mode_str, err);

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    if (!ten_msg_conversion_per_property_rule_fixed_value_from_json(
            &self->u.fixed_value, json, err)) {
      ten_msg_conversion_per_property_rule_destroy(self);
      return NULL;
    }
    break;

  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    ten_msg_conversion_per_property_rule_from_original_from_json(
        &self->u.from_original, json);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return self;
}

static const char *
ten_msg_conversion_per_property_rule_conversion_mode_to_string(
    TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE conversion_mode,
    ten_error_t *err) {
  switch (conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    return TEN_STR_FIXED_VALUE;
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    return TEN_STR_FROM_ORIGINAL;
  default:
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Unsupported conversion mode '%d'", conversion_mode);
    }
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

bool ten_msg_conversion_per_property_rule_to_json(
    ten_msg_conversion_per_property_rule_t *self, ten_json_t *json,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_json_t rule_json = TEN_JSON_INIT_VAL(json->ctx, false);
  ten_json_init_object(&rule_json);
  ten_json_array_append(json, &rule_json);

  ten_json_object_set_string(
      &rule_json, TEN_STR_CONVERSION_MODE,
      ten_msg_conversion_per_property_rule_conversion_mode_to_string(
          self->conversion_mode, err));

  ten_json_object_set_string(&rule_json, TEN_STR_PATH,
                             ten_string_get_raw_str(&self->property_path));

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    if (!ten_msg_conversion_per_property_rule_fixed_value_to_json(
            &self->u.fixed_value, &rule_json, err)) {
      return false;
    }
    break;

  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    if (!ten_msg_conversion_per_property_rule_from_original_to_json(
            &self->u.from_original, &rule_json, err)) {
      return false;
    }
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}

ten_msg_conversion_per_property_rule_t *
ten_msg_conversion_per_property_rule_from_value(ten_value_t *value,
                                                ten_error_t *err) {
  TEN_ASSERT(value, "Invalid argument.");

  ten_msg_conversion_per_property_rule_t *self =
      ten_msg_conversion_per_property_rule_create();

  ten_string_set_formatted(
      &self->property_path, "%s",
      ten_value_peek_raw_str(ten_value_object_peek(value, TEN_STR_PATH), err));

  const char *conversion_mode_str = ten_value_peek_raw_str(
      ten_value_object_peek(value, TEN_STR_CONVERSION_MODE), err);

  self->conversion_mode =
      ten_msg_conversion_per_property_rule_conversion_mode_from_string(
          conversion_mode_str, err);

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    if (!ten_msg_conversion_per_property_rule_fixed_value_from_value(
            &self->u.fixed_value, value, err)) {
      ten_msg_conversion_per_property_rule_destroy(self);
      self = NULL;
    }
    break;

  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    if (!ten_msg_conversion_per_property_rule_from_original_from_value(
            &self->u.from_original, value, err)) {
      ten_msg_conversion_per_property_rule_destroy(self);
      self = NULL;
    }
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return self;
}

TEN_RUNTIME_PRIVATE_API ten_value_t *
ten_msg_conversion_per_property_rule_to_value(
    ten_msg_conversion_per_property_rule_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_t *result = ten_value_create_object_with_move(NULL);

  ten_list_push_ptr_back(
      &result->content.object,
      ten_value_kv_create(
          TEN_STR_CONVERSION_MODE,
          ten_value_create_string(
              ten_msg_conversion_per_property_rule_conversion_mode_to_string(
                  self->conversion_mode, err))),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_list_push_ptr_back(
      &result->content.object,
      ten_value_kv_create(TEN_STR_PATH,
                          ten_value_create_string(
                              ten_string_get_raw_str(&self->property_path))),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  switch (self->conversion_mode) {
  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FIXED_VALUE:
    ten_msg_conversion_per_property_rule_fixed_value_to_value(
        &self->u.fixed_value, result);
    break;

  case TEN_MSG_CONVERSION_PER_PROPERTY_RULE_CONVERSION_MODE_FROM_ORIGINAL:
    ten_msg_conversion_per_property_rule_from_original_to_value(
        &self->u.from_original, result);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return result;
}
