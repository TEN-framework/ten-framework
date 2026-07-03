//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_and_result_conversion.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/base.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

static ten_msg_and_result_conversion_t *ten_msg_and_result_conversion_create(
    void) {
  ten_msg_and_result_conversion_t *self =
      (ten_msg_and_result_conversion_t *)TEN_MALLOC(
          sizeof(ten_msg_and_result_conversion_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->msg = NULL;
  self->result = NULL;

  return self;
}

void ten_msg_and_result_conversion_destroy(
    ten_msg_and_result_conversion_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->msg) {
    ten_msg_conversion_destroy(self->msg);
  }
  if (self->result) {
    ten_msg_conversion_destroy(self->result);
  }

  TEN_FREE(self);
}

ten_msg_and_result_conversion_t *ten_msg_and_result_conversion_from_json(
    ten_json_t *json, ten_error_t *err) {
  ten_msg_and_result_conversion_t *pair =
      ten_msg_and_result_conversion_create();

  pair->msg = ten_msg_conversion_from_json(json, err);
  if (!pair->msg) {
    ten_msg_and_result_conversion_destroy(pair);
    return NULL;
  }

  ten_json_t result_json = TEN_JSON_INIT_VAL(json->ctx, false);
  bool success = ten_json_object_peek(json, TEN_STR_RESULT, &result_json);
  if (success) {
    pair->result = ten_msg_conversion_from_json(&result_json, err);
    if (!pair->result) {
      ten_msg_and_result_conversion_destroy(pair);
      return NULL;
    }
  }

  return pair;
}

bool ten_msg_and_result_conversion_to_json(
    ten_msg_and_result_conversion_t *self, ten_json_t *json, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  bool success = ten_msg_conversion_to_json(self->msg, json, err);
  if (!success) {
    return false;
  }

  if (self->result) {
    ten_json_t result_operation_json = TEN_JSON_INIT_VAL(json->ctx, false);
    ten_json_object_peek_or_create_object(json, TEN_STR_RESULT,
                                          &result_operation_json);

    success =
        ten_msg_conversion_to_json(self->result, &result_operation_json, err);
    if (!success) {
      return false;
    }
  }

  return true;
}

ten_msg_and_result_conversion_t *ten_msg_and_result_conversion_from_value(
    ten_value_t *value, ten_error_t *err) {
  ten_msg_and_result_conversion_t *pair =
      ten_msg_and_result_conversion_create();

  pair->msg = ten_msg_conversion_from_value(value, err);
  if (!pair->msg) {
    ten_msg_and_result_conversion_destroy(pair);
    return NULL;
  }

  ten_value_t *resp_value = ten_value_object_peek(value, TEN_STR_RESULT);
  if (resp_value) {
    pair->result = ten_msg_conversion_from_value(resp_value, err);
    if (!pair->result) {
      ten_msg_and_result_conversion_destroy(pair);
      return NULL;
    }
  }

  return pair;
}

ten_value_t *ten_msg_and_result_conversion_to_value(
    ten_msg_and_result_conversion_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_t *msg_and_result_conversion_value =
      ten_msg_conversion_to_value(self->msg, err);
  if (!msg_and_result_conversion_value) {
    return NULL;
  }

  if (self->result) {
    ten_value_t *result_operation_value =
        ten_msg_conversion_to_value(self->result, err);
    if (!result_operation_value) {
      ten_value_destroy(msg_and_result_conversion_value);
      return NULL;
    }

    ten_value_kv_t *kv =
        ten_value_kv_create(TEN_STR_RESULT, result_operation_value);

    ten_list_push_ptr_back(
        &msg_and_result_conversion_value->content.object, kv,
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  return msg_and_result_conversion_value;
}
