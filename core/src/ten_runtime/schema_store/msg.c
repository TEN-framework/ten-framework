//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/schema_store/msg.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/schema_store/property.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"

bool ten_msg_schema_check_integrity(ten_msg_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_MSG_SCHEMA_SIGNATURE) {
    return false;
  }

  return true;
}

void ten_msg_schema_init(ten_msg_schema_t *self,
                         ten_value_t *msg_schema_value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg_schema_value && ten_value_check_integrity(msg_schema_value),
             "Invalid argument.");

  if (!ten_value_is_object(msg_schema_value)) {
    TEN_ASSERT(0, "The schema should be an object.");
    return;
  }

  ten_signature_set(&self->signature, TEN_MSG_SCHEMA_SIGNATURE);

  const char *msg_name =
      ten_value_object_peek_string(msg_schema_value, TEN_STR_NAME);
  if (!msg_name) {
    msg_name = TEN_STR_MSG_NAME_TEN_EMPTY;
  }

  ten_string_init_formatted(&self->msg_name, "%s", msg_name);
  self->property =
      ten_schemas_parse_schema_object_for_property(msg_schema_value);
}

void ten_msg_schema_deinit(ten_msg_schema_t *self) {
  TEN_ASSERT(self && ten_msg_schema_check_integrity(self), "Invalid argument.");

  ten_signature_set(&self->signature, 0);
  ten_string_deinit(&self->msg_name);
  if (self->property) {
    ten_schema_destroy(self->property);
    self->property = NULL;
  }
}

ten_msg_schema_t *ten_msg_schema_create(ten_value_t *msg_schema_value) {
  TEN_ASSERT(msg_schema_value && ten_value_check_integrity(msg_schema_value),
             "Invalid argument.");

  ten_msg_schema_t *self = TEN_MALLOC(sizeof(ten_msg_schema_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_msg_schema_init(self, msg_schema_value);

  return self;
}

void ten_msg_schema_destroy(ten_msg_schema_t *self) {
  TEN_ASSERT(self && ten_msg_schema_check_integrity(self), "Invalid argument.");

  ten_msg_schema_deinit(self);
  TEN_FREE(self);
}

bool ten_msg_schema_adjust_properties(ten_msg_schema_t *self,
                                      ten_value_t *msg_props,
                                      ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(msg_props && ten_value_check_integrity(msg_props),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  if (!self->property) {
    // No `property` schema is defined, which is permitted in TEN runtime.
    return true;
  }

  return ten_schema_adjust_value_type(self->property, msg_props, err);
}

bool ten_msg_schema_validate_properties(ten_msg_schema_t *self,
                                        ten_value_t *msg_props,
                                        ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(msg_props && ten_value_check_integrity(msg_props),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  if (!self->property) {
    // No `property` schema is defined, which is permitted in TEN runtime.
    return true;
  }

  return ten_schema_validate_value(self->property, msg_props, err);
}

const char *ten_msg_schema_get_msg_name(ten_msg_schema_t *self) {
  TEN_ASSERT(self && ten_msg_schema_check_integrity(self), "Invalid argument.");

  return ten_string_get_raw_str(&self->msg_name);
}
