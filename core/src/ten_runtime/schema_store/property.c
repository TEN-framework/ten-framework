//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/schema_store/property.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_utils/schema/constant_str.h"
#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "include_internal/ten_utils/value/constant_str.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"
#include "ten_utils/value/value_merge.h"
#include "ten_utils/value/value_object.h"

static void ten_schema_object_kv_destroy_key_only(ten_value_kv_t *self) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");

  ten_string_deinit(&self->key);

  // Value is not owned by the key-value pair, do not destroy it.
  self->value = NULL;
  TEN_FREE(self);
}

ten_schema_t *ten_schemas_parse_schema_object_for_property(
    ten_value_t *schemas_content) {
  TEN_ASSERT(schemas_content && ten_value_check_integrity(schemas_content),
             "Invalid argument.");

  if (!ten_value_is_object(schemas_content)) {
    TEN_ASSERT(0, "The schema should be an object contains `property`.");
    return NULL;
  }

  ten_value_t *property_schema_content =
      ten_value_object_peek(schemas_content, TEN_STR_PROPERTY);
  if (!property_schema_content) {
    return NULL;
  }

  // The structure of the `schemas_content` is not a standard `object` schema,
  // create a new object schema for the property schema.
  ten_list_t object_schema_fields = TEN_LIST_INIT_VAL;

  ten_list_push_ptr_back(
      &object_schema_fields,
      ten_value_kv_create(TEN_SCHEMA_KEYWORD_STR_TYPE,
                          ten_value_create_string(TEN_STR_OBJECT)),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_list_push_ptr_back(
      &object_schema_fields,
      ten_value_kv_create(TEN_SCHEMA_KEYWORD_STR_PROPERTIES,
                          property_schema_content),
      (ten_ptr_listnode_destroy_func_t)ten_schema_object_kv_destroy_key_only);

  ten_value_t *required_schema_content =
      ten_value_object_peek(schemas_content, TEN_SCHEMA_KEYWORD_STR_REQUIRED);
  if (required_schema_content) {
    ten_list_push_ptr_back(
        &object_schema_fields,
        ten_value_kv_create(TEN_SCHEMA_KEYWORD_STR_REQUIRED,
                            required_schema_content),
        (ten_ptr_listnode_destroy_func_t)ten_schema_object_kv_destroy_key_only);
  }

  ten_value_t *object_schema_content =
      ten_value_create_object_with_move(&object_schema_fields);
  ten_list_clear(&object_schema_fields);

  ten_schema_t *schema = ten_schema_create_from_value(object_schema_content);
  TEN_ASSERT(schema, "Should not happen.");

  ten_value_destroy(object_schema_content);

  return schema;
}
