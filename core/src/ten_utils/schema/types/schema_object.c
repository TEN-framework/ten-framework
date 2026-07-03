//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/types/schema_object.h"

#include "include_internal/ten_utils/schema/keywords/keyword_properties.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_schema_object_check_integrity(ten_schema_object_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_SCHEMA_OBJECT_SIGNATURE) {
    return false;
  }

  return true;
}

ten_schema_object_t *ten_schema_object_create(void) {
  ten_schema_object_t *self = TEN_MALLOC(sizeof(ten_schema_object_t));
  if (!self) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_OBJECT_SIGNATURE);
  ten_schema_init(&self->hdr);
  self->keyword_properties = NULL;
  self->keyword_required = NULL;

  return self;
}

void ten_schema_object_destroy(ten_schema_object_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_object_check_integrity(self), "Invalid argument.");

  ten_signature_set(&self->signature, 0);

  // The 'keyword_properties' will be destroyed from 'self->hdr.keywords'.
  ten_schema_deinit(&self->hdr);
  self->keyword_properties = NULL;
  self->keyword_required = NULL;
  TEN_FREE(self);
}

ten_schema_t *ten_schema_object_peek_property_schema(ten_schema_t *self,
                                                     const char *prop_name) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(prop_name, "Invalid argument.");

  ten_schema_object_t *object_schema = (ten_schema_object_t *)self;
  TEN_ASSERT(ten_schema_object_check_integrity(object_schema),
             "Invalid argument.");

  return ten_schema_keyword_properties_peek_property_schema(
      object_schema->keyword_properties, prop_name);
}
