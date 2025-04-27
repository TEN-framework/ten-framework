//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"

#define TEN_SCHEMA_OBJECT_PROPERTY_SIGNATURE 0x48C4F08A95436D91U
#define TEN_SCHEMA_KEYWORD_PROPERTIES_SIGNATURE 0xB115E3DEF5E41A12U

typedef struct ten_schema_object_property_t {
  ten_signature_t signature;
  ten_hashhandle_t hh_in_properties_table;
  ten_string_t name;  // The name of the property in the object.
  ten_schema_t *schema;
} ten_schema_object_property_t;

typedef struct ten_schema_keyword_properties_t {
  ten_schema_keyword_t hdr;
  ten_signature_t signature;

  // The properties of the object. The type of the value is
  // ten_schema_object_property_t.
  ten_hashtable_t properties;
} ten_schema_keyword_properties_t;

TEN_UTILS_PRIVATE_API bool ten_schema_keyword_properties_check_integrity(
    ten_schema_keyword_properties_t *self);

TEN_UTILS_PRIVATE_API ten_schema_keyword_t *
ten_schema_keyword_properties_create_from_value(ten_schema_t *owner,
                                                ten_value_t *value);

TEN_UTILS_PRIVATE_API ten_schema_t *
ten_schema_keyword_properties_peek_property_schema(
    ten_schema_keyword_properties_t *self, const char *prop_name);
