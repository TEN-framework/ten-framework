//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword_type.h"

#include <stdbool.h>

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "include_internal/ten_utils/value/value_convert.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/type_operation.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"

bool ten_schema_keyword_type_check_integrity(ten_schema_keyword_type_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (!ten_schema_keyword_check_integrity(&self->hdr)) {
    return false;
  }

  if (ten_signature_get(&self->signature) !=
      TEN_SCHEMA_KEYWORD_TYPE_SIGNATURE) {
    return false;
  }

  if (self->type == TEN_TYPE_INVALID) {
    return false;
  }

  return true;
}

static void ten_schema_keyword_type_destroy(ten_schema_keyword_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_schema_keyword_type_t *self = (ten_schema_keyword_type_t *)self_;
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(self),
             "Invalid argument.");

  ten_schema_keyword_deinit(&self->hdr);
  self->type = TEN_TYPE_INVALID;

  TEN_FREE(self);
}

static bool ten_schema_keyword_type_validate_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value, "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_type_t *self = (ten_schema_keyword_type_t *)self_;
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(self),
             "Invalid argument.");

  TEN_TYPE value_type = ten_value_get_type(value);
  if (!ten_type_is_compatible(value_type, self->type)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value type does not match the schema type, given: %s, "
                  "expected: %s",
                  ten_type_to_string(value_type),
                  ten_type_to_string(self->type));
    return false;
  }

  return true;
}

// Automatically perform TEN-supported value conversion based on the type. This
// conversion is generally safe, meaning the value will not change as a result.
static bool ten_schema_keyword_type_adjust_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value, "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_type_t *self = (ten_schema_keyword_type_t *)self_;
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(self),
             "Invalid argument.");

  TEN_TYPE schema_type = self->type;
  TEN_TYPE value_type = ten_value_get_type(value);
  if (value_type == schema_type) {
    // The types are consistent, so there's no need to adjust the value; this is
    // a normal situation.
    return true;
  }

  ten_error_t *err = schema_err->err;
  switch (schema_type) {
  case TEN_TYPE_INT8:
    return ten_value_convert_to_int8(value, err);
  case TEN_TYPE_INT16:
    return ten_value_convert_to_int16(value, err);
  case TEN_TYPE_INT32:
    return ten_value_convert_to_int32(value, err);
  case TEN_TYPE_INT64:
    return ten_value_convert_to_int64(value, err);
  case TEN_TYPE_UINT8:
    return ten_value_convert_to_uint8(value, err);
  case TEN_TYPE_UINT16:
    return ten_value_convert_to_uint16(value, err);
  case TEN_TYPE_UINT32:
    return ten_value_convert_to_uint32(value, err);
  case TEN_TYPE_UINT64:
    return ten_value_convert_to_uint64(value, err);
  case TEN_TYPE_FLOAT32:
    return ten_value_convert_to_float32(value, err);
  case TEN_TYPE_FLOAT64:
    return ten_value_convert_to_float64(value, err);
  default:
    // Note that the format of error message should be same as the above
    // functions.
    ten_error_set(
        err, TEN_ERROR_CODE_GENERIC, "unsupported conversion from `%s` to `%s`",
        ten_type_to_string(value_type), ten_type_to_string(self->type));
    return false;
  }
}

// Type compatibility:
// The target type has a larger range of values than the source type.
//
// Note that the `self` and `target` type keyword should not be NULL, otherwise
// their schemas are invalid.
static bool ten_schema_keyword_type_is_compatible(
    ten_schema_keyword_t *self_, ten_schema_keyword_t *target_,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && target_, "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_type_t *self = (ten_schema_keyword_type_t *)self_;
  ten_schema_keyword_type_t *target = (ten_schema_keyword_type_t *)target_;
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(target),
             "Invalid argument.");

  bool success = ten_type_is_compatible(self->type, target->type);
  if (!success) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "type is incompatible, source is [%s], but target is [%s]",
                  ten_type_to_string(self->type),
                  ten_type_to_string(target->type));
  }

  return success;
}

static ten_schema_keyword_type_t *ten_schema_keyword_type_create(
    TEN_TYPE type, ten_schema_t *schema) {
  ten_schema_keyword_type_t *self =
      TEN_MALLOC(sizeof(ten_schema_keyword_type_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_schema_keyword_init(&self->hdr, TEN_SCHEMA_KEYWORD_TYPE);
  self->hdr.destroy = ten_schema_keyword_type_destroy;
  self->hdr.owner = schema;
  self->hdr.validate_value = ten_schema_keyword_type_validate_value;
  self->hdr.adjust_value = ten_schema_keyword_type_adjust_value;
  self->hdr.is_compatible = ten_schema_keyword_type_is_compatible;

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_TYPE_SIGNATURE);
  self->type = type;

  return self;
}

ten_schema_keyword_t *ten_schema_keyword_type_create_from_value(
    ten_schema_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && value, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(owner), "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");

  if (!ten_value_is_string(value)) {
    TEN_ASSERT(0, "The schema type should be string.");
    return NULL;
  }

  TEN_TYPE type = ten_type_from_string(ten_value_peek_raw_str(value, NULL));
  if (type == TEN_TYPE_INVALID) {
    TEN_ASSERT(0, "Invalid TEN type.");
    return NULL;
  }

  owner->keyword_type = ten_schema_keyword_type_create(type, owner);
  return &owner->keyword_type->hdr;
}
