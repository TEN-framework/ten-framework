//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword_required.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "include_internal/ten_utils/schema/types/schema_object.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_str.h"
#include "ten_utils/container/list_str.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_object.h"

bool ten_schema_keyword_required_check_integrity(
    ten_schema_keyword_required_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      TEN_SCHEMA_KEYWORD_REQUIRED_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_schema_keyword_required_destroy(ten_schema_keyword_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_schema_keyword_required_t *self = (ten_schema_keyword_required_t *)self_;
  TEN_ASSERT(ten_schema_keyword_required_check_integrity(self),
             "Invalid argument.");

  ten_list_clear(&self->required_properties);
  ten_schema_keyword_deinit(&self->hdr);
  TEN_FREE(self);
}

static bool ten_schema_keyword_required_validate_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(ten_schema_error_check_integrity(schema_err), "Invalid argument.");

  if (!ten_value_is_object(value)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value should be an object");
    return false;
  }

  ten_schema_keyword_required_t *self = (ten_schema_keyword_required_t *)self_;
  TEN_ASSERT(ten_schema_keyword_required_check_integrity(self),
             "Invalid argument.");

  if (ten_list_size(&self->required_properties) == 0) {
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  ten_string_t absent_keys;
  TEN_STRING_INIT(absent_keys);

  ten_list_foreach (&self->required_properties, iter) {
    ten_string_t *required_property = ten_str_listnode_get(iter.node);
    TEN_ASSERT(
        required_property && ten_string_check_integrity(required_property),
        "Should not happen.");

    ten_value_t *expected_to_be_present =
        ten_value_object_peek(value, ten_string_get_raw_str(required_property));
    if (!expected_to_be_present) {
      bool is_last =
          ten_list_size(&self->required_properties) == iter.index + 1;
      ten_string_append_formatted(&absent_keys, is_last ? "'%s'" : "'%s', ",
                                  ten_string_get_raw_str(required_property));
    }
  }

  bool result = true;
  if (ten_string_len(&absent_keys) > 0) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the required properties are absent: %s",
                  ten_string_get_raw_str(&absent_keys));
    result = false;
  }

  ten_string_deinit(&absent_keys);

  return result;
}

static bool ten_schema_keyword_required_adjust_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");

  // There is no need to adjust the value for the schema keyword 'required'.
  return true;
}

// Required compatibility:
// 1. The source collection needs to be a superset of the target collection.
// 2. Or the target required keyword is undefined.
static bool ten_schema_keyword_required_is_compatible(
    ten_schema_keyword_t *self_, ten_schema_keyword_t *target_,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  if (!self_) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the `required` in the source schema is undefined");
    return false;
  }

  if (!target_) {
    return true;
  }

  ten_schema_keyword_required_t *self = (ten_schema_keyword_required_t *)self_;
  ten_schema_keyword_required_t *target =
      (ten_schema_keyword_required_t *)target_;

  TEN_ASSERT(ten_schema_keyword_required_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_required_check_integrity(target),
             "Invalid argument.");

  if (ten_list_size(&self->required_properties) <
      ten_list_size(&target->required_properties)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "required is incompatible, the size of the source can not be "
                  "less than the target.");
    return false;
  }

  ten_string_t missing_keys;
  TEN_STRING_INIT(missing_keys);

  ten_list_foreach (&target->required_properties, iter) {
    ten_string_t *target_required = ten_str_listnode_get(iter.node);
    TEN_ASSERT(target_required && ten_string_check_integrity(target_required),
               "Should not happen.");

    if (!ten_list_find_string(&self->required_properties,
                              ten_string_get_raw_str(target_required))) {
      ten_string_append_formatted(&missing_keys, "'%s', ",
                                  ten_string_get_raw_str(target_required));
    }
  }

  bool success = ten_string_is_empty(&missing_keys);
  if (!success) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "required is incompatible, the properties [%s] are defined "
                  "in the source but not in the target",
                  ten_string_get_raw_str(&missing_keys));
  }

  ten_string_deinit(&missing_keys);

  return success;
}

static ten_schema_keyword_required_t *ten_schema_keyword_required_create(
    ten_schema_object_t *owner) {
  TEN_ASSERT(owner, "Invalid argument.");

  ten_schema_keyword_required_t *self =
      TEN_MALLOC(sizeof(ten_schema_keyword_required_t));
  if (!self) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_REQUIRED_SIGNATURE);
  ten_list_init(&self->required_properties);

  ten_schema_keyword_init(&self->hdr, TEN_SCHEMA_KEYWORD_REQUIRED);

  self->hdr.owner = &owner->hdr;
  self->hdr.destroy = ten_schema_keyword_required_destroy;
  self->hdr.validate_value = ten_schema_keyword_required_validate_value;
  self->hdr.adjust_value = ten_schema_keyword_required_adjust_value;
  self->hdr.is_compatible = ten_schema_keyword_required_is_compatible;

  owner->keyword_required = self;

  return self;
}

ten_schema_keyword_t *ten_schema_keyword_required_create_from_value(
    ten_schema_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && ten_schema_check_integrity(owner), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  if (!ten_value_is_array(value)) {
    TEN_ASSERT(0, "The schema keyword 'required' should be an array.");
    return NULL;
  }

  ten_schema_object_t *schema_object = (ten_schema_object_t *)owner;
  TEN_ASSERT(schema_object && ten_schema_object_check_integrity(schema_object),
             "Invalid argument.");

  ten_schema_keyword_required_t *self =
      ten_schema_keyword_required_create(schema_object);
  if (!self) {
    return NULL;
  }

  bool has_error = false;

  ten_value_array_foreach(value, iter) {
    ten_value_t *item = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(item && ten_value_check_integrity(item), "Should not happen.");

    if (!ten_value_is_string(item)) {
      TEN_ASSERT(0,
                 "The schema keyword 'required' should be an array of "
                 "strings.");
      has_error = true;
      break;
    }

    const char *required_property = ten_value_peek_raw_str(item, NULL);
    TEN_ASSERT(required_property, "Should not happen.");

    ten_list_push_str_back(&self->required_properties, required_property);
  }

  if (has_error) {
    ten_schema_keyword_required_destroy(&self->hdr);
    return NULL;
  }

  if (ten_list_size(&self->required_properties) == 0) {
    ten_schema_keyword_required_destroy(&self->hdr);
    TEN_ASSERT(0, "The schema keyword 'required' should not be empty.");
    return NULL;
  }

  return &self->hdr;
}
