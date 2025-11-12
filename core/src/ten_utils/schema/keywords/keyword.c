//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword.h"

#include "ten_utils/macro/check.h"

bool ten_schema_keyword_check_integrity(ten_schema_keyword_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_SCHEMA_KEYWORD_SIGNATURE) {
    return false;
  }

  if (self->type <= TEN_SCHEMA_KEYWORD_INVALID ||
      self->type >= TEN_SCHEMA_KEYWORD_LAST) {
    return false;
  }

  if (!self->destroy) {
    return false;
  }

  return true;
}

void ten_schema_keyword_init(ten_schema_keyword_t *self,
                             TEN_SCHEMA_KEYWORD type) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      type > TEN_SCHEMA_KEYWORD_INVALID && type < TEN_SCHEMA_KEYWORD_LAST,
      "Invalid argument.");

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_SIGNATURE);

  self->type = type;
  self->owner = NULL;
  self->destroy = NULL;
  self->validate_value = NULL;
  self->adjust_value = NULL;
  self->is_compatible = NULL;
}

void ten_schema_keyword_deinit(ten_schema_keyword_t *self) {
  TEN_ASSERT(self && ten_schema_keyword_check_integrity(self),
             "Invalid argument.");

  self->type = TEN_SCHEMA_KEYWORD_INVALID;
  self->owner = NULL;
  self->destroy = NULL;
  self->validate_value = NULL;
  self->adjust_value = NULL;
}
