//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/keywords/keyword_properties.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "include_internal/ten_utils/schema/types/schema_object.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/type_operation.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

bool ten_schema_keyword_properties_check_integrity(
    ten_schema_keyword_properties_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      TEN_SCHEMA_KEYWORD_PROPERTIES_SIGNATURE) {
    return false;
  }

  if (!ten_schema_keyword_check_integrity(&self->hdr)) {
    return false;
  }

  return true;
}

static void ten_schema_keyword_properties_destroy(ten_schema_keyword_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_schema_keyword_properties_t *self =
      (ten_schema_keyword_properties_t *)self_;
  TEN_ASSERT(ten_schema_keyword_properties_check_integrity(self),
             "Invalid argument.");

  ten_signature_set(&self->signature, 0);
  ten_hashtable_deinit(&self->properties);
  ten_schema_keyword_deinit(&self->hdr);
  TEN_FREE(self);
}

ten_schema_t *ten_schema_keyword_properties_peek_property_schema(
    ten_schema_keyword_properties_t *self, const char *prop_name) {
  TEN_ASSERT(self && ten_schema_keyword_properties_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(prop_name && strlen(prop_name), "Invalid argument.");

  ten_hashhandle_t *hh =
      ten_hashtable_find_string(&self->properties, prop_name);
  if (hh) {
    ten_schema_object_property_t *property_schema =
        CONTAINER_OF_FROM_OFFSET(hh, self->properties.hh_offset);
    TEN_ASSERT(property_schema, "Should not happen.");
    return property_schema->schema;
  }

  return NULL;
}

static bool ten_schema_keyword_properties_validate_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_check_integrity(self_), "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(ten_schema_error_check_integrity(schema_err), "Invalid argument.");

  if (!ten_value_is_object(value)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value should be an object, but is: %s",
                  ten_type_to_string(ten_value_get_type(value)));
    return false;
  }

  ten_schema_keyword_properties_t *self =
      (ten_schema_keyword_properties_t *)self_;
  TEN_ASSERT(self && ten_schema_keyword_properties_check_integrity(self),
             "Invalid argument.");

  // Only check the fields the `value` has, but not all the fields defined in
  // the schema. In other words, the default value of the `required` keyword is
  // empty.
  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Should not happen.");

    ten_string_t *field_key = &kv->key;
    ten_value_t *field_value = kv->value;
    ten_schema_t *prop_schema =
        ten_schema_keyword_properties_peek_property_schema(
            self, ten_string_get_raw_str(field_key));
    if (!prop_schema) {
      // The schema of some property might not be defined, it's ok. Using the
      // `required` keyword to check if the property is required.
      continue;
    }

    if (!ten_schema_validate_value_with_schema_error(prop_schema, field_value,
                                                     schema_err)) {
      ten_string_prepend_formatted(&schema_err->path, ".%s",
                                   ten_string_get_raw_str(field_key));
      return false;
    }
  }

  return true;
}

static bool ten_schema_keyword_properties_adjust_value(
    ten_schema_keyword_t *self_, ten_value_t *value,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && value && schema_err, "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_check_integrity(self_), "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(ten_schema_error_check_integrity(schema_err), "Invalid argument.");

  if (!ten_value_is_object(value)) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC,
                  "the value should be an object, but is: %s",
                  ten_type_to_string(ten_value_get_type(value)));
    return false;
  }

  ten_schema_keyword_properties_t *self =
      (ten_schema_keyword_properties_t *)self_;
  TEN_ASSERT(self && ten_schema_keyword_properties_check_integrity(self),
             "Invalid argument.");

  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Should not happen.");

    ten_string_t *field_key = &kv->key;
    ten_value_t *field_value = kv->value;
    ten_schema_t *prop_schema =
        ten_schema_keyword_properties_peek_property_schema(
            self, ten_string_get_raw_str(field_key));
    if (!prop_schema) {
      continue;
    }

    if (!ten_schema_adjust_value_type_with_schema_error(
            prop_schema, field_value, schema_err)) {
      ten_string_prepend_formatted(&schema_err->path, ".%s",
                                   ten_string_get_raw_str(field_key));
      return false;
    }
  }

  return true;
}

// Properties compatibility:
// The type of the same property in the source collection should be compatible
// with the target.
//
// Note that the `self` and `target` properties keyword should not be NULL,
// otherwise their schemas are invalid.
static bool ten_schema_keyword_properties_is_compatible(
    ten_schema_keyword_t *self_, ten_schema_keyword_t *target_,
    ten_schema_error_t *schema_err) {
  TEN_ASSERT(self_ && target_, "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_schema_keyword_properties_t *self =
      (ten_schema_keyword_properties_t *)self_;
  TEN_ASSERT(self && ten_schema_keyword_properties_check_integrity(self),
             "Invalid argument.");

  ten_schema_keyword_properties_t *target =
      (ten_schema_keyword_properties_t *)target_;
  TEN_ASSERT(target && ten_schema_keyword_properties_check_integrity(target),
             "Invalid argument.");

  ten_string_t incompatible_fields;
  TEN_STRING_INIT(incompatible_fields);

  ten_hashtable_foreach(&self->properties, iter) {
    ten_schema_object_property_t *property =
        CONTAINER_OF_FROM_OFFSET(iter.node, self->properties.hh_offset);
    TEN_ASSERT(property, "Should not happen.");

    ten_schema_t *target_prop_schema =
        ten_schema_keyword_properties_peek_property_schema(
            target, ten_string_get_raw_str(&property->name));
    if (!target_prop_schema) {
      continue;
    }

    if (!ten_schema_is_compatible_with_schema_error(
            property->schema, target_prop_schema, schema_err)) {
      // Ex:
      // .a[0]: type is incompatible, ...; .b: ...
      const char *separator =
          ten_string_is_empty(&incompatible_fields) ? "" : "; ";
      ten_string_append_formatted(&incompatible_fields, "%s.%s%s: %s",
                                  separator,
                                  ten_string_get_raw_str(&property->name),
                                  ten_string_get_raw_str(&schema_err->path),
                                  ten_error_message(schema_err->err));
    }

    ten_schema_error_reset(schema_err);
  }

  bool success = ten_string_is_empty(&incompatible_fields);

  if (!success) {
    ten_error_set(schema_err->err, TEN_ERROR_CODE_GENERIC, "{ %s }",
                  ten_string_get_raw_str(&incompatible_fields));
  }

  ten_string_deinit(&incompatible_fields);

  return success;
}

static ten_schema_keyword_properties_t *ten_schema_keyword_properties_create(
    ten_schema_object_t *owner) {
  ten_schema_keyword_properties_t *self =
      TEN_MALLOC(sizeof(ten_schema_keyword_properties_t));
  if (self == NULL) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_KEYWORD_PROPERTIES_SIGNATURE);

  ten_schema_keyword_init(&self->hdr, TEN_SCHEMA_KEYWORD_PROPERTIES);
  self->hdr.destroy = ten_schema_keyword_properties_destroy;
  self->hdr.owner = &owner->hdr;
  self->hdr.validate_value = ten_schema_keyword_properties_validate_value;
  self->hdr.adjust_value = ten_schema_keyword_properties_adjust_value;
  self->hdr.is_compatible = ten_schema_keyword_properties_is_compatible;

  ten_hashtable_init(&self->properties, offsetof(ten_schema_object_property_t,
                                                 hh_in_properties_table));

  owner->keyword_properties = self;

  return self;
}

static ten_schema_object_property_t *ten_schema_object_property_create(
    const char *name, ten_value_t *value) {
  ten_schema_object_property_t *self =
      TEN_MALLOC(sizeof(ten_schema_object_property_t));
  if (self == NULL) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_signature_set(&self->signature, TEN_SCHEMA_OBJECT_PROPERTY_SIGNATURE);
  ten_string_init_formatted(&self->name, "%s", name);
  self->schema = ten_schema_create_from_value(value);
  if (!self->schema) {
    TEN_ASSERT(0, "Failed to parse schema for property %s.", name);
    return NULL;
  }

  return self;
}

static void ten_schema_object_property_destroy(
    ten_schema_object_property_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_schema_destroy(self->schema);
  ten_string_deinit(&self->name);
  ten_signature_set(&self->signature, 0);
  TEN_FREE(self);
}

static void ten_schema_keyword_properties_append_item(
    ten_schema_keyword_properties_t *self,
    ten_schema_object_property_t *property) {
  TEN_ASSERT(self && property, "Invalid argument.");

  ten_hashtable_add_string(&self->properties, &property->hh_in_properties_table,
                           ten_string_get_raw_str(&property->name),
                           ten_schema_object_property_destroy);
}

ten_schema_keyword_t *ten_schema_keyword_properties_create_from_value(
    ten_schema_t *owner, ten_value_t *value) {
  TEN_ASSERT(owner && value, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(owner), "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");

  if (!ten_value_is_object(value)) {
    TEN_ASSERT(0, "The schema keyword properties must be an object.");
    return NULL;
  }

  ten_schema_object_t *schema_object = (ten_schema_object_t *)owner;
  TEN_ASSERT(ten_schema_object_check_integrity(schema_object),
             "Invalid argument.");

  ten_schema_keyword_properties_t *keyword_properties =
      ten_schema_keyword_properties_create(schema_object);

  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *field_kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(field_kv && ten_value_kv_check_integrity(field_kv),
               "Should not happen.");

    ten_string_t *field_key = &field_kv->key;
    ten_value_t *field_value = field_kv->value;

    ten_schema_object_property_t *property = ten_schema_object_property_create(
        ten_string_get_raw_str(field_key), field_value);
    if (!property) {
      TEN_ASSERT(property, "Invalid schema property at %s.",
                 ten_string_get_raw_str(field_key));
      return NULL;
    }

    ten_schema_keyword_properties_append_item(keyword_properties, property);
  }

  return &keyword_properties->hdr;
}
