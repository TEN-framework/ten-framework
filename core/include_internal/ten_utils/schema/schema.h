//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/value/value.h"

#define TEN_SCHEMA_SIGNATURE 0x4D9FEA8F6273C974U
#define TEN_SCHEMA_ERROR_SIGNATURE 0x32B696D4FC8FFD09U

typedef struct ten_schema_keyword_type_t ten_schema_keyword_type_t;

// A schema definition describes the structure and validation rules for a TEN
// value.
//
// Examples of schema definitions:
//
// {
//   "type": "object",
//   "properties": {
//     "a": {
//       "type": "string"
//     },
//     "b": {
//       "type": "uint16"
//     }
//   }
// }
//
// {
//   "type": "array",
//   "items": {
//     "type": "int64"
//   }
// }
//
// A schema consists of three main components:
//
// 1. Type - Corresponds to the value type and is represented by
//    ten_schema_keyword_type_t. This defines the basic structure of the schema
//    (object, array, primitive).
//
// 2. Children - For complex types like objects (properties) and arrays (items).
//    Primitive types don't have children.
//
// 3. Validation rules - Represented by ten_schema_keyword_t instances.
//    Examples include minimum/maximum values for integers, string length
//    constraints, etc.
//
// The schema structure varies by value type, so we define different schema
// types. The ten_schema_t serves as the base class for all schema types, with
// the following inheritance hierarchy:
//
//                         ┌─────────────────┐
//                         │   ten_schema_t  │
//                         └────────┬────────┘
//                                  │
//                  ┌───────────────┼───────────────┐
//                  │               │               │
//    ┌─────────────┴────────┐ ┌────┴─────────┐ ┌───┴────────────┐
//    │       primitive_t    │ │   object_t   │ │    array_t     │
//    └──────────────────────┘ └──────────────┘ └────────────────┘
typedef struct ten_schema_t {
  ten_signature_t signature;

  // Maps keyword identifiers to ten_schema_keyword_t instances.
  // Key is TEN_SCHEMA_KEYWORD, value is ten_schema_keyword_t*.
  ten_hashtable_t keywords;  // TEN_SCHEMA_KEYWORD -> ten_schema_keyword_t*

  // Cache field for quick access to the type keyword.
  // This points to a `ten_schema_keyword_t` that exists in the `keywords` map
  // above. By directly accessing this field instead of searching the `keywords`
  // map, we improve performance. Similar caching mechanisms are used in derived
  // schema types like `ten_schema_object_t` and `ten_schema_array_t`. The
  // `keywords` map handles resource management, while this field provides
  // efficient access.
  ten_schema_keyword_type_t *keyword_type;
} ten_schema_t;

/**
 * @brief Error context structure used during schema validation.
 *
 * This structure is designed for internal use to track validation errors and
 * maintain path information during schema validation. It allows for precise
 * error reporting by recording the exact location where validation failed
 * within nested data structures.
 *
 * Example schema:
 * ```json
 * {
 *   "type": "object",
 *   "properties": {
 *     "a": {
 *       "type": "array",
 *       "items": {
 *         "type": "int32"
 *       }
 *     }
 *   }
 * }
 * ```
 *
 * Example value to validate:
 * ```json
 * {
 *   "a": [1, "2", 3]
 * }
 * ```
 *
 * During validation, each value is verified against its corresponding schema
 * in depth-first search order until an error is encountered. In this example,
 * the error would occur at `a[1]` because "2" is a string, not an int32.
 *
 * The path tracking is necessary because:
 * 1. The standard `ten_error_t` structure doesn't have space to store path
 *    information.
 * 2. The schema structure itself (`ten_schema_t`) cannot record the path
 *    because it lacks index information for array elements (all items in an
 *    array share the same schema definition).
 *
 * This structure bridges that gap by maintaining both the error details and the
 * path to the error location.
 */
typedef struct ten_schema_error_t {
  ten_signature_t signature;  // Integrity verification signature.
  ten_error_t *err;           // Pointer to the error object containing details.
  ten_string_t path;          // Path to the location where validation failed.
} ten_schema_error_t;

TEN_UTILS_PRIVATE_API bool ten_schema_error_check_integrity(
    ten_schema_error_t *self);

TEN_UTILS_PRIVATE_API void ten_schema_error_init(ten_schema_error_t *self,
                                                 ten_error_t *err);

TEN_UTILS_PRIVATE_API void ten_schema_error_deinit(ten_schema_error_t *self);

TEN_UTILS_PRIVATE_API void ten_schema_error_reset(ten_schema_error_t *self);

TEN_UTILS_PRIVATE_API bool ten_schema_check_integrity(ten_schema_t *self);

TEN_UTILS_PRIVATE_API void ten_schema_init(ten_schema_t *self);

TEN_UTILS_PRIVATE_API void ten_schema_deinit(ten_schema_t *self);

TEN_UTILS_PRIVATE_API ten_schema_t *ten_schema_create_from_json_str(
    const char *json_str, const char **err_msg);

TEN_UTILS_PRIVATE_API bool ten_schema_adjust_and_validate_json_str(
    ten_schema_t *self, const char *json_str, const char **err_msg);

TEN_UTILS_API ten_schema_t *ten_schema_create_from_json(ten_json_t *json);

TEN_UTILS_API ten_schema_t *ten_schema_create_from_value(ten_value_t *value);

TEN_UTILS_API void ten_schema_destroy(ten_schema_t *self);

TEN_UTILS_PRIVATE_API bool ten_schema_validate_value_with_schema_error(
    ten_schema_t *self, ten_value_t *value, ten_schema_error_t *schema_err);

TEN_UTILS_API bool ten_schema_validate_value(ten_schema_t *self,
                                             ten_value_t *value,
                                             ten_error_t *err);

TEN_UTILS_PRIVATE_API bool ten_schema_adjust_value_type_with_schema_error(
    ten_schema_t *self, ten_value_t *value, ten_schema_error_t *schema_err);

TEN_UTILS_API bool ten_schema_adjust_value_type(ten_schema_t *self,
                                                ten_value_t *value,
                                                ten_error_t *err);

TEN_UTILS_PRIVATE_API bool ten_schema_is_compatible_with_schema_error(
    ten_schema_t *self, ten_schema_t *target, ten_schema_error_t *schema_err);

TEN_UTILS_API bool ten_schema_is_compatible(ten_schema_t *self,
                                            ten_schema_t *target,
                                            ten_error_t *err);
