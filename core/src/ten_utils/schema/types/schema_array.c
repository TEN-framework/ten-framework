//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/types/schema_array.h"

#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_schema_array_check_integrity(ten_schema_array_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_SCHEMA_ARRAY_SIGNATURE) {
    return false;
  }

  if (!ten_schema_check_integrity(&self->hdr)) {
    return false;
  }

  return true;
}

ten_schema_array_t *ten_schema_array_create(void) {
  ten_schema_array_t *self = TEN_MALLOC(sizeof(ten_schema_array_t));
  if (self == NULL) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_ARRAY_SIGNATURE);
  ten_schema_init(&self->hdr);
  self->keyword_items = NULL;

  return self;
}

void ten_schema_array_destroy(ten_schema_array_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_array_check_integrity(self), "Invalid argument.");

  ten_signature_set(&self->signature, 0);

  // The 'keyword_items' will be destroyed from 'self->hdr.keywords'.
  ten_schema_deinit(&self->hdr);
  self->keyword_items = NULL;
  TEN_FREE(self);
}
