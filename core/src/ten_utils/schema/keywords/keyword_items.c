//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword_items.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "include_internal/ten_utils/schema/types/schema_array.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/type_operation.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"

bool ten_schema_keyword_items_check_integrity(
    ten_schema_keyword_items_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      TEN_SCHEMA_KEYWORD_ITEMS_SIGNATURE) {
    return false;
  }

  if (!ten_schema_keyword_check_integrity(&self->hdr)) {
    return false;
  }

  return true;
}

static bool ten_schema_keyword_items_validate_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && ten_schema_keyword_check_integrity(self_),
             "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  if (!ten_value_is_array(value)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value should be an array, but is: %s",
                  ten_type_to_string(ten_value_get_type(value)));
    return false;
  }

  ten_schema_keyword_items_t *self = (ten_schema_keyword_items_t *)self_;
  TEN_ASSERT(ten_schema_keyword_items_check_integrity(self),
             "Invalid argument.");

  if (!self->item_schema) {
    TEN_ASSERT(0, "Should not happen.");
    return true;
  }

  ten_value_array_foreach(value, value_iter) {
    ten_value_t *value_field = ten_ptr_listnode_get(value_iter.node);
    TEN_ASSERT(value_field && ten_value_check_integrity(value_field),
               "Invalid argument.");

    if (!ten_schema_validate_value_with_schema_error(self->item_schema,
                                                     value_field, schema_err)) {
      ten_string_prepend_formatted(&schema_err->path, "[%d]", value_iter.index);
      return false;
    }
  }

  return true;
}

static void ten_schema_keyword_items_destroy(ten_schema_keyword_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_schema_keyword_items_t *self = (ten_schema_keyword_items_t *)self_;
  TEN_ASSERT(ten_schema_keyword_items_check_integrity(self),
             "Invalid argument.");

  ten_signature_set(&self->signature, 0);
  ten_schema_keyword_deinit(&self->hdr);
  ten_schema_destroy(self->item_schema);
  TEN_FREE(self);
}

static bool ten_schema_keyword_items_adjust_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && ten_schema_keyword_check_integrity(self_),
             "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_items_t *self = (ten_schema_keyword_items_t *)self_;
  TEN_ASSERT(ten_schema_keyword_items_check_integrity(self),
             "Invalid argument.");

  if (!ten_value_is_array(value)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value should be an array, but is: %s",
                  ten_type_to_string(ten_value_get_type(value)));
    return false;
  }

  if (!self->item_schema) {
    TEN_ASSERT(0, "Should not happen.");
    return true;
  }

  ten_value_array_foreach(value, value_iter) {
    ten_value_t *value_field = ten_ptr_listnode_get(value_iter.node);
    TEN_ASSERT(value_field && ten_value_check_integrity(value_field),
               "Invalid argument.");

    if (!ten_schema_adjust_value_type_with_schema_error(
            self->item_schema, value_field, schema_err)) {
      ten_string_prepend_formatted(&schema_err->path, "[%d]", value_iter.index);
      return false;
    }
  }

  return true;
}

// Items compatibility:
// 1. The source collection needs to be a subset of the target collection (Not
// support, there is no item count information now).
// 2. The type of the same property in the source collection should be
// compatible with the target.
//
// Note that the `self` and `target` items keyword should not be NULL, otherwise
// their schemas are invalid.
static bool ten_schema_keyword_items_is_compatible(
    ten_schema_keyword_t *self_, ten_schema_keyword_t *target_,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && target_, "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_items_t *self = (ten_schema_keyword_items_t *)self_;
  TEN_ASSERT(ten_schema_keyword_items_check_integrity(self),
             "Invalid argument.");

  ten_schema_keyword_items_t *target = (ten_schema_keyword_items_t *)target_;
  TEN_ASSERT(ten_schema_keyword_items_check_integrity(target),
             "Invalid argument.");

  bool success = ten_schema_is_compatible_with_schema_error(
      self->item_schema, target->item_schema, schema_err);
  if (!success) {
    ten_string_prepend_formatted(&schema_err->path, "[]");
  }

  return success;
}

static ten_schema_keyword_items_t *ten_schema_keyword_items_create(
    ten_schema_array_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && ten_schema_array_check_integrity(owner),
             "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_schema_keyword_items_t *self =
      TEN_MALLOC(sizeof(ten_schema_keyword_items_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_ITEMS_SIGNATURE);

  ten_schema_keyword_init(&self->hdr, TEN_SCHEMA_KEYWORD_ITEMS);
  self->hdr.owner = &owner->hdr;
  self->hdr.destroy = ten_schema_keyword_items_destroy;
  self->hdr.validate_value = ten_schema_keyword_items_validate_value;
  self->hdr.adjust_value = ten_schema_keyword_items_adjust_value;
  self->hdr.is_compatible = ten_schema_keyword_items_is_compatible;

  self->item_schema = ten_schema_create_from_value(value);
  if (!self->item_schema) {
    TEN_ASSERT(0, "Failed to parse schema keyword [items].");
    return NULL;
  }

  return self;
}

ten_schema_keyword_t *ten_schema_keyword_items_create_from_value(
    ten_schema_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && ten_schema_check_integrity(owner), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  if (!ten_value_is_object(value)) {
    TEN_ASSERT(0, "The schema keyword `items` must be an object.");
    return NULL;
  }

  ten_schema_array_t *schema_array = (ten_schema_array_t *)owner;
  TEN_ASSERT(schema_array && ten_schema_array_check_integrity(schema_array),
             "Invalid argument.");

  ten_schema_keyword_items_t *keyword_items =
      ten_schema_keyword_items_create(schema_array, value);
  if (!keyword_items) {
    return NULL;
  }

  schema_array->keyword_items = keyword_items;

  return &keyword_items->hdr;
}
