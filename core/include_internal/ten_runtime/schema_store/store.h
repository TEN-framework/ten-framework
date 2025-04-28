//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/value/value.h"

#define TEN_SCHEMA_STORE_SIGNATURE 0x0FD9B508D67169A4U

typedef struct ten_schema_t ten_schema_t;
typedef struct ten_msg_schema_t ten_msg_schema_t;

typedef struct ten_schema_store_t {
  ten_signature_t signature;

  // The schema definitions are as follows:
  //
  // "api": {                    <== This section will be passed to
  //                                 `ten_schema_store_init`.
  //   "property": {
  //     "prop_a": {
  //       "type": "string"
  //     },
  //     "prop_b": {
  //       "type": "uint8"
  //     }
  //   }
  // }
  //
  // The type of property schema is always ten_schema_object_t, refer to
  // `ten_schemas_parse_schema_object_for_property`.
  ten_schema_t *property;

  // Key is the cmd name, the type of value is `ten_cmd_schema_t`.
  ten_hashtable_t cmd_in;
  ten_hashtable_t cmd_out;

  // Key is the msg name, the type of value is `ten_msg_schema_t`.
  ten_hashtable_t data_in;
  ten_hashtable_t data_out;
  ten_hashtable_t video_frame_in;
  ten_hashtable_t video_frame_out;
  ten_hashtable_t audio_frame_in;
  ten_hashtable_t audio_frame_out;
} ten_schema_store_t;

TEN_RUNTIME_PRIVATE_API bool ten_schema_store_check_integrity(
    ten_schema_store_t *self);

TEN_RUNTIME_API void ten_schema_store_init(ten_schema_store_t *self);

TEN_RUNTIME_API bool ten_schema_store_set_schema_definition(
    ten_schema_store_t *self, ten_value_t *schema_def, ten_error_t *err);

TEN_RUNTIME_API void ten_schema_store_deinit(ten_schema_store_t *self);

/**
 * @param props_value The property must be an object.
 */
TEN_RUNTIME_API bool ten_schema_store_validate_properties(
    ten_schema_store_t *self, ten_value_t *props_value, ten_error_t *err);

/**
 * @param prop_name The property name must not be NULL or empty.
 */
TEN_RUNTIME_API bool ten_schema_store_validate_property_kv(
    ten_schema_store_t *self, const char *prop_name, ten_value_t *prop_value,
    ten_error_t *err);

/**
 * @param prop_name The property name must not be NULL or empty.
 */
TEN_RUNTIME_API bool ten_schema_store_adjust_property_kv(
    ten_schema_store_t *self, const char *prop_name, ten_value_t *prop_value,
    ten_error_t *err);

/**
 * @param props_value The property must be an object.
 */
TEN_RUNTIME_API bool ten_schema_store_adjust_properties(
    ten_schema_store_t *self, ten_value_t *props_value, ten_error_t *err);

TEN_RUNTIME_API ten_msg_schema_t *ten_schema_store_get_msg_schema(
    ten_schema_store_t *self, TEN_MSG_TYPE msg_type, const char *msg_name,
    bool is_msg_out);
