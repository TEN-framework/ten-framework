//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/schema_store/cmd.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/schema_store/property.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_object.h"

bool ten_cmd_schema_check_integrity(ten_cmd_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_CMD_SCHEMA_SIGNATURE) {
    return false;
  }

  return true;
}

ten_cmd_schema_t *ten_cmd_schema_create(ten_value_t *cmd_schema_value) {
  TEN_ASSERT(cmd_schema_value && ten_value_check_integrity(cmd_schema_value),
             "Invalid argument.");

  if (!ten_value_is_object(cmd_schema_value)) {
    TEN_ASSERT(0, "The schema should be an object.");
    return NULL;
  }

  ten_cmd_schema_t *self = TEN_MALLOC(sizeof(ten_cmd_schema_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_CMD_SCHEMA_SIGNATURE);
  self->cmd_result_schema = NULL;

  ten_msg_schema_init(&self->hdr, cmd_schema_value);

  // "api": {
  //   "cmd_in": [
  //     {
  //       "name": "cmd_foo",
  //       "property": {
  //         "foo": {
  //           "type": "string"
  //         }
  //       },
  //       "result": {  <==
  //         "property": {
  //           "status_foo": {
  //             "type": "uint8"
  //           }
  //         }
  //       }
  //     }
  //   ]
  // }
  ten_value_t *result = ten_value_object_peek(cmd_schema_value, TEN_STR_RESULT);
  if (!result) {
    TEN_LOGD("No schema [result] found for cmd [%s].",
             ten_value_object_peek_string(cmd_schema_value, TEN_STR_NAME));
    return self;
  }

  if (!ten_value_is_object(result)) {
    TEN_ASSERT(0, "The schema [result] should be an object.");

    ten_cmd_schema_destroy(self);
    return NULL;
  }

  self->cmd_result_schema =
      ten_schemas_parse_schema_object_for_property(result);

  return self;
}

void ten_cmd_schema_destroy(ten_cmd_schema_t *self) {
  TEN_ASSERT(self && ten_cmd_schema_check_integrity(self), "Invalid argument.");

  ten_signature_set(&self->signature, 0);

  ten_msg_schema_deinit(&self->hdr);
  if (self->cmd_result_schema) {
    ten_schema_destroy(self->cmd_result_schema);
    self->cmd_result_schema = NULL;
  }

  TEN_FREE(self);
}

ten_string_t *ten_cmd_schema_get_cmd_name(ten_cmd_schema_t *self) {
  TEN_ASSERT(self && ten_cmd_schema_check_integrity(self), "Invalid argument.");

  return &self->hdr.msg_name;
}

bool ten_cmd_schema_validate_cmd_result_properties(
    ten_cmd_schema_t *self, ten_value_t *cmd_result_props, ten_error_t *err) {
  TEN_ASSERT(self && ten_cmd_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(cmd_result_props && ten_value_check_integrity(cmd_result_props),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  if (!self->cmd_result_schema) {
    // No `result` schema is defined, which is permitted in TEN runtime.
    return true;
  }

  return ten_schema_validate_value(self->cmd_result_schema, cmd_result_props,
                                   err);
}

bool ten_cmd_schema_adjust_cmd_result_properties(ten_cmd_schema_t *self,
                                                 ten_value_t *cmd_result_props,
                                                 ten_error_t *err) {
  TEN_ASSERT(self && ten_cmd_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(cmd_result_props && ten_value_check_integrity(cmd_result_props),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  if (!self->cmd_result_schema) {
    // No `result` schema is defined, which is permitted in TEN runtime.
    return true;
  }

  return ten_schema_adjust_value_type(self->cmd_result_schema, cmd_result_props,
                                      err);
}
