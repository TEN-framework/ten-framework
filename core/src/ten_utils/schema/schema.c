//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/schema.h"

#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/keywords/keyword_type.h"
#include "include_internal/ten_utils/schema/keywords/keywords_info.h"
#include "include_internal/ten_utils/schema/types/schema_array.h"
#include "include_internal/ten_utils/schema/types/schema_object.h"
#include "include_internal/ten_utils/schema/types/schema_primitive.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/type_operation.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_object.h"

bool ten_schema_error_check_integrity(ten_schema_error_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_SCHEMA_ERROR_SIGNATURE) {
    return false;
  }

  if (!self->err) {
    return false;
  }

  return true;
}

void ten_schema_error_init(ten_schema_error_t *self, ten_error_t *err) {
  TEN_ASSERT(self && err, "Invalid argument.");

  ten_signature_set(&self->signature, TEN_SCHEMA_ERROR_SIGNATURE);
  self->err = err;
  TEN_STRING_INIT(self->path);
}

void ten_schema_error_deinit(ten_schema_error_t *self) {
  TEN_ASSERT(self && ten_schema_error_check_integrity(self),
             "Invalid argument.");

  ten_signature_set(&self->signature, 0);
  self->err = NULL;
  ten_string_deinit(&self->path);
}

void ten_schema_error_reset(ten_schema_error_t *self) {
  TEN_ASSERT(self && ten_schema_error_check_integrity(self),
             "Invalid argument.");

  ten_string_clear(&self->path);
  ten_error_reset(self->err);
}

bool ten_schema_check_integrity(ten_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_SCHEMA_SIGNATURE) {
    return false;
  }

  return true;
}

void ten_schema_init(ten_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, TEN_SCHEMA_SIGNATURE);
  self->keyword_type = NULL;
  ten_hashtable_init(&self->keywords,
                     offsetof(ten_schema_keyword_t, hh_in_keyword_map));
}

void ten_schema_deinit(ten_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");

  ten_signature_set(&self->signature, 0);
  ten_hashtable_deinit(&self->keywords);
}

static ten_schema_t *ten_schema_create_by_type(const char *type) {
  TEN_ASSERT(type && strlen(type) > 0, "Invalid argument.");

  TEN_TYPE schema_type = ten_type_from_string(type);
  switch (schema_type) {
  case TEN_TYPE_OBJECT: {
    ten_schema_object_t *self = ten_schema_object_create();
    if (!self) {
      return NULL;
    }

    return &self->hdr;
  }

  case TEN_TYPE_ARRAY: {
    ten_schema_array_t *self = ten_schema_array_create();
    if (!self) {
      return NULL;
    }

    return &self->hdr;
  }

  case TEN_TYPE_INT8:
  case TEN_TYPE_INT16:
  case TEN_TYPE_INT32:
  case TEN_TYPE_INT64:
  case TEN_TYPE_UINT8:
  case TEN_TYPE_UINT16:
  case TEN_TYPE_UINT32:
  case TEN_TYPE_UINT64:
  case TEN_TYPE_FLOAT32:
  case TEN_TYPE_FLOAT64:
  case TEN_TYPE_BOOL:
  case TEN_TYPE_STRING:
  case TEN_TYPE_BUF:
  case TEN_TYPE_PTR: {
    ten_schema_primitive_t *self = ten_schema_primitive_create();
    if (!self) {
      return NULL;
    }

    return &self->hdr;
  }

  default:
    TEN_ASSERT(0, "Invalid schema type, %s.", type);
    return NULL;
  }
}

static void ten_schema_append_keyword(ten_schema_t *self,
                                      ten_schema_keyword_t *keyword) {
  TEN_ASSERT(self && keyword, "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_check_integrity(keyword), "Invalid argument.");

  ten_hashtable_add_int(&self->keywords, &keyword->hh_in_keyword_map,
                        (int32_t *)&keyword->type, keyword->destroy);
}

ten_schema_t *ten_schema_create_from_json(ten_json_t *json) {
  TEN_ASSERT(json && ten_json_is_object(json), "Invalid argument.");

  ten_value_t *value = ten_value_from_json(json);
  TEN_ASSERT(value && ten_value_is_object(value), "Should not happen.");

  ten_schema_t *self = ten_schema_create_from_value(value);
  ten_value_destroy(value);

  return self;
}

ten_schema_t *ten_schema_create_from_value(ten_value_t *value) {
  TEN_ASSERT(value && ten_value_is_object(value), "Invalid argument.");

  const char *schema_type =
      ten_value_object_peek_string(value, TEN_SCHEMA_KEYWORD_STR_TYPE);
  if (schema_type == NULL) {
    TEN_ASSERT(0, "The schema should have a type.");
    return NULL;
  }

  ten_schema_t *self = ten_schema_create_by_type(schema_type);
  if (!self) {
    return NULL;
  }

  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *field_kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(field_kv && ten_value_kv_check_integrity(field_kv),
               "Should not happen.");

    ten_string_t *field_key = &field_kv->key;
    ten_value_t *field_value = field_kv->value;

    const ten_schema_keyword_info_t *keyword_info =
        ten_schema_keyword_info_get_by_name(ten_string_get_raw_str(field_key));
    TEN_ASSERT(keyword_info, "Should not happen.");

    if (keyword_info->from_value == NULL) {
      TEN_ASSERT(0, "Should not happen.");
      continue;
    }

    ten_schema_keyword_t *keyword = keyword_info->from_value(self, field_value);
    TEN_ASSERT(keyword, "Should not happen.");
    TEN_ASSERT(ten_schema_keyword_check_integrity(keyword),
               "Should not happen.");

    ten_schema_append_keyword(self, keyword);
  }

  return self;
}

void ten_schema_destroy(ten_schema_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");

  ten_schema_keyword_type_t *keyword_type = self->keyword_type;
  TEN_ASSERT(keyword_type, "Invalid argument.");
  TEN_ASSERT(ten_schema_keyword_type_check_integrity(keyword_type),
             "Invalid argument.");

  switch (keyword_type->type) {
  case TEN_TYPE_OBJECT: {
    ten_schema_object_t *schema_object = (ten_schema_object_t *)self;
    TEN_ASSERT(ten_schema_object_check_integrity(schema_object),
               "Invalid argument.");

    ten_schema_object_destroy(schema_object);
    break;
  }

  case TEN_TYPE_ARRAY: {
    ten_schema_array_t *schema_array = (ten_schema_array_t *)self;
    TEN_ASSERT(ten_schema_array_check_integrity(schema_array),
               "Invalid argument.");

    ten_schema_array_destroy(schema_array);
    break;
  }

  default: {
    ten_schema_primitive_t *schema_primitive = (ten_schema_primitive_t *)self;
    TEN_ASSERT(ten_schema_primitive_check_integrity(schema_primitive),
               "Invalid argument.");

    ten_schema_primitive_destroy(schema_primitive);
    break;
  }
  }
}

bool ten_schema_validate_value_with_schema_error(
    ten_schema_t *self, ten_value_t *value, ten_schema_error_t *schema_err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  ten_hashtable_foreach(&self->keywords, iter) {
    ten_schema_keyword_t *keyword = CONTAINER_OF_FROM_OFFSET(
        iter.node, offsetof(ten_schema_keyword_t, hh_in_keyword_map));
    TEN_ASSERT(keyword, "Should not happen.");
    TEN_ASSERT(ten_schema_keyword_check_integrity(keyword),
               "Should not happen.");

    bool success = keyword->validate_value(keyword, value, schema_err);
    if (!success) {
      return false;
    }
  }

  return true;
}

/**
 * Validates a value against a schema.
 *
 * This function checks if the provided value conforms to the schema's
 * requirements. It handles error reporting and provides detailed error
 * messages including the path to the problematic element in case of validation
 * failure.
 *
 * @param self The schema to validate against.
 * @param value The value to be validated.
 * @param err Error context to store validation errors. If NULL, a temporary
 *            error context will be created.
 * @return true if the value is valid according to the schema, false otherwise.
 */
bool ten_schema_validate_value(ten_schema_t *self, ten_value_t *value,
                               ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");

  // Create a temporary error context if none was provided.
  bool new_err = false;
  if (!err) {
    err = ten_error_create();
    new_err = true;
  } else {
    TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");
  }

  bool result = false;
  if (!value) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Value is required.");
    goto done;
  }

  // Initialize schema error context for path tracking during validation.
  ten_schema_error_t err_ctx;
  ten_schema_error_init(&err_ctx, err);

  // Perform the actual validation with path tracking.
  result = ten_schema_validate_value_with_schema_error(self, value, &err_ctx);

  // If validation failed and we have a path, prepend it to the error message.
  if (!result && !ten_string_is_empty(&err_ctx.path)) {
    ten_error_prepend_error_message(
        err, "%s: ", ten_string_get_raw_str(&err_ctx.path));
  }

  ten_schema_error_deinit(&err_ctx);

done:
  // Clean up temporary error context if we created one.
  if (new_err) {
    ten_error_destroy(err);
  }

  return result;
}

/**
 * Adjusts a value's type according to the schema with detailed error reporting.
 *
 * This function iterates through all schema keywords and applies their type
 * adjustment logic to the provided value. It uses a schema_err context to
 * provide detailed error information including the path to the problematic
 * element in case of failure.
 *
 * @param self The schema to use for type adjustment.
 * @param value The value to be adjusted according to schema requirements.
 * @param schema_err Error context that tracks the path and detailed error
 * information.
 * @return true if all adjustments succeeded or no adjustment was needed, false
 * otherwise.
 */
bool ten_schema_adjust_value_type_with_schema_error(
    ten_schema_t *self, ten_value_t *value, ten_schema_error_t *schema_err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  // Iterate through all keywords in the schema.
  ten_hashtable_foreach(&self->keywords, iter) {
    // Extract the keyword from the hash table node.
    ten_schema_keyword_t *keyword = CONTAINER_OF_FROM_OFFSET(
        iter.node, offsetof(ten_schema_keyword_t, hh_in_keyword_map));
    TEN_ASSERT(keyword, "Should not happen.");
    TEN_ASSERT(ten_schema_keyword_check_integrity(keyword),
               "Should not happen.");

    // Apply the keyword's adjustment logic to the value.
    bool success = keyword->adjust_value(keyword, value, schema_err);
    if (!success) {
      // Stop processing if any adjustment fails.
      return false;
    }
  }

  // All adjustments succeeded.
  return true;
}

/**
 * Adjusts a value's type according to the schema.
 *
 * This function attempts to adjust the provided value to conform to the type
 * requirements specified in the schema. For example, it might convert a string
 * representation of a number into an actual number value if the schema expects
 * a numeric type.
 *
 * @param self The schema to use for type adjustment.
 * @param value The value to be adjusted.
 * @param err Error context to report any issues (can be NULL).
 * @return true if adjustment succeeded or no adjustment was needed, false
 * otherwise.
 */
bool ten_schema_adjust_value_type(ten_schema_t *self, ten_value_t *value,
                                  ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");

  // Create a new error context if none was provided.
  bool new_err = false;
  if (!err) {
    err = ten_error_create();
    new_err = true;
  } else {
    TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");
  }

  bool result = false;

  // Validate that we have a value to adjust.
  if (!value) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Value is required.");
    goto done;
  }

  // Initialize schema error context for detailed error reporting.
  ten_schema_error_t err_ctx;
  ten_schema_error_init(&err_ctx, err);

  // Perform the actual type adjustment using the schema.
  result =
      ten_schema_adjust_value_type_with_schema_error(self, value, &err_ctx);

  // If adjustment failed, prepend the path to the error message for better
  // context.
  if (!result && !ten_string_is_empty(&err_ctx.path)) {
    ten_error_prepend_error_message(
        err, "%s: ", ten_string_get_raw_str(&err_ctx.path));
  }

  // Clean up the schema error context.
  ten_schema_error_deinit(&err_ctx);

done:
  // Clean up the error context if we created it.
  if (new_err) {
    ten_error_destroy(err);
  }

  return result;
}

static ten_schema_keyword_t *ten_schema_peek_keyword_by_type(
    ten_schema_t *self, TEN_SCHEMA_KEYWORD keyword_type) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(keyword_type > TEN_SCHEMA_KEYWORD_INVALID &&
                 keyword_type < TEN_SCHEMA_KEYWORD_LAST,
             "Invalid argument.");

  ten_hashhandle_t *hh =
      ten_hashtable_find_int(&self->keywords, (int32_t *)&keyword_type);
  if (!hh) {
    return NULL;
  }

  return CONTAINER_OF_FROM_OFFSET(hh, self->keywords.hh_offset);
}

bool ten_schema_is_compatible_with_schema_error(
    ten_schema_t *self, ten_schema_t *target, ten_schema_error_t *schema_err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(target && ten_schema_check_integrity(target), "Invalid argument.");
  TEN_ASSERT(schema_err && ten_schema_error_check_integrity(schema_err),
             "Invalid argument.");

  bool result = true;

  // The schema 'type' should be checked first, there is no need to check other
  // keywords if the type is incompatible.
  for (TEN_SCHEMA_KEYWORD keyword_type = TEN_SCHEMA_KEYWORD_TYPE;
       keyword_type < TEN_SCHEMA_KEYWORD_LAST; keyword_type++) {
    ten_schema_keyword_t *source_keyword =
        ten_schema_peek_keyword_by_type(self, keyword_type);
    ten_schema_keyword_t *target_keyword =
        ten_schema_peek_keyword_by_type(target, keyword_type);

    // It's OK if some source keyword or target keyword is missing, such as
    // 'required' keyword; if the source schema has 'required' keyword but the
    // target does not, it's compatible.
    if (source_keyword) {
      result = source_keyword->is_compatible(source_keyword, target_keyword,
                                             schema_err);
    } else if (target_keyword) {
      result = target_keyword->is_compatible(NULL, target_keyword, schema_err);
    } else {
      continue;
    }

    if (!result) {
      break;
    }
  }

  return result;
}

bool ten_schema_is_compatible(ten_schema_t *self, ten_schema_t *target,
                              ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(target && ten_schema_check_integrity(target), "Invalid argument.");

  bool new_err = false;
  if (!err) {
    err = ten_error_create();
    new_err = true;
  } else {
    TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");
  }

  ten_schema_error_t err_ctx;
  ten_schema_error_init(&err_ctx, err);

  bool result =
      ten_schema_is_compatible_with_schema_error(self, target, &err_ctx);
  if (!result && !ten_string_is_empty(&err_ctx.path)) {
    ten_error_prepend_error_message(
        err, "%s: ", ten_string_get_raw_str(&err_ctx.path));
  }

  ten_schema_error_deinit(&err_ctx);

  if (new_err) {
    ten_error_destroy(err);
  }

  return result;
}

ten_schema_t *ten_schema_create_from_json_str(const char *json_string,
                                              const char **err_msg) {
  TEN_ASSERT(json_string, "Invalid argument.");

  ten_schema_t *schema = NULL;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_json_t *schema_json = ten_json_from_string(json_string, &err);
  do {
    if (!schema_json) {
      break;
    }

    if (!ten_json_is_object(schema_json)) {
      ten_error_set(&err, TEN_ERROR_CODE_GENERIC, "Invalid schema json.");
      break;
    }

    schema = ten_schema_create_from_json(schema_json);
  } while (0);

  if (schema_json) {
    ten_json_destroy(schema_json);
  }

  if (!ten_error_is_success(&err)) {
    *err_msg = TEN_STRDUP(ten_error_message(&err));
  }

  ten_error_deinit(&err);

  return schema;
}

/**
 * Adjusts and validates a JSON string against a schema.
 *
 * This function performs two operations:
 * 1. Adjusts the value types to match the schema requirements.
 * 2. Validates the adjusted value against the schema.
 *
 * @param self The schema to validate against.
 * @param json_string The JSON string to validate.
 * @param err_msg Pointer to store error message if validation fails.
 * @return true if the JSON is valid according to the schema, false otherwise.
 */
bool ten_schema_adjust_and_validate_json_str(ten_schema_t *self,
                                             const char *json_string,
                                             const char **err_msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(json_string, "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Parse the JSON string into a ten_json_t structure.
  ten_json_t *json = ten_json_from_string(json_string, &err);

  ten_value_t *value = NULL;
  do {
    // Break if JSON parsing failed.
    if (!json) {
      break;
    }

    // Convert the JSON to a TEN value.
    value = ten_value_from_json(json);
    if (!value) {
      ten_error_set(&err, TEN_ERROR_CODE_GENERIC, "Failed to parse JSON.");
      break;
    }

    // Adjust the value types to match schema requirements.
    if (!ten_schema_adjust_value_type(self, value, &err)) {
      break;
    }

    // Validate the adjusted value against the schema.
    if (!ten_schema_validate_value(self, value, &err)) {
      break;
    }
  } while (0);

  // Clean up resources.
  if (json) {
    ten_json_destroy(json);
  }

  if (value) {
    ten_value_destroy(value);
  }

  // Set error message if validation failed.
  bool result = ten_error_is_success(&err);
  if (!result) {
    *err_msg = TEN_STRDUP(ten_error_message(&err));
  }

  ten_error_deinit(&err);

  return result;
}
