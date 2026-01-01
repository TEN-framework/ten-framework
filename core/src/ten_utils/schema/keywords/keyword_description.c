//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword_description.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"

bool ten_schema_keyword_description_check_integrity(
    ten_schema_keyword_description_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      TEN_SCHEMA_KEYWORD_DESCRIPTION_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_schema_keyword_description_destroy(
    ten_schema_keyword_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_schema_keyword_description_t *self =
      (ten_schema_keyword_description_t *)self_;
  TEN_ASSERT(ten_schema_keyword_description_check_integrity(self),
             "Invalid argument.");

  ten_schema_keyword_deinit(&self->hdr);
  TEN_FREE(self);
}

static bool ten_schema_keyword_description_validate_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(ten_schema_error_check_integrity(schema_err), "Invalid argument.");

  // The 'description' keyword is for documentation purposes only.
  // It does not affect validation.
  return true;
}

static bool ten_schema_keyword_description_adjust_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");

  // There is no need to adjust the value for the schema keyword 'description'.
  return true;
}

static bool ten_schema_keyword_description_is_compatible(
    ten_schema_keyword_t *self_, ten_schema_keyword_t *target_,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  // The 'description' keyword is for documentation purposes only.
  // It does not affect compatibility.
  return true;
}

static ten_schema_keyword_description_t *ten_schema_keyword_description_create(
    ten_schema_t *owner) {
  TEN_ASSERT(owner, "Invalid argument.");

  ten_schema_keyword_description_t *self =
      TEN_MALLOC(sizeof(ten_schema_keyword_description_t));
  if (!self) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_DESCRIPTION_SIGNATURE);

  ten_schema_keyword_init(&self->hdr, TEN_SCHEMA_KEYWORD_DESCRIPTION);

  self->hdr.owner = owner;
  self->hdr.destroy = ten_schema_keyword_description_destroy;
  self->hdr.validate_value = ten_schema_keyword_description_validate_value;
  self->hdr.adjust_value = ten_schema_keyword_description_adjust_value;
  self->hdr.is_compatible = ten_schema_keyword_description_is_compatible;

  return self;
}

ten_schema_keyword_t *ten_schema_keyword_description_create_from_value(
    ten_schema_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && ten_schema_check_integrity(owner), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  // The 'description' can be a string or an object (localizedText).
  // We just need to accept it without processing the content.
  if (!ten_value_is_string(value) && !ten_value_is_object(value)) {
    TEN_ASSERT(0,
               "The schema keyword 'description' should be a string or an "
               "object.");
    return NULL;
  }

  ten_schema_keyword_description_t *self =
      ten_schema_keyword_description_create(owner);
  if (!self) {
    return NULL;
  }

  return &self->hdr;
}
