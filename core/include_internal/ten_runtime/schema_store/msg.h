//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/value/value.h"

#define TEN_MSG_SCHEMA_SIGNATURE 0x5E2D4490ADD96568U

typedef struct ten_msg_schema_t {
  ten_signature_t signature;

  ten_string_t msg_name;
  ten_hashhandle_t hh_in_map;

  // The schema definitions are as follows:
  //
  // "api": {
  //   "data_in": [
  //     {                <== This section will be passed to
  //                          `ten_msg_schema_create`.
  //       "name": "foo",
  //       "property": {  <== This section will be stored in `property`.
  //         "foo": {
  //           "type": "string"
  //         },
  //         "bar": {
  //           "type": "int8"
  //         }
  //       }
  //     }
  //   ]
  // }
  //
  // The actual type is ten_schema_object_t, refer to
  // 'ten_schemas_parse_schema_object_for_property'.
  ten_schema_t *property;
} ten_msg_schema_t;

TEN_RUNTIME_PRIVATE_API bool ten_msg_schema_check_integrity(
    ten_msg_schema_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_schema_t *ten_msg_schema_create(
    ten_value_t *msg_schema_value);

TEN_RUNTIME_PRIVATE_API void ten_msg_schema_destroy(ten_msg_schema_t *self);

TEN_RUNTIME_PRIVATE_API void ten_msg_schema_init(ten_msg_schema_t *self,
                                                 ten_value_t *msg_schema_value);

TEN_RUNTIME_PRIVATE_API void ten_msg_schema_deinit(ten_msg_schema_t *self);

/**
 * @param msg_props The property must be an object.
 */
TEN_RUNTIME_API bool ten_msg_schema_adjust_properties(ten_msg_schema_t *self,
                                                      ten_value_t *msg_props,
                                                      ten_error_t *err);

/**
 * @param msg_props The property must be an object.
 */
TEN_RUNTIME_API bool ten_msg_schema_validate_properties(ten_msg_schema_t *self,
                                                        ten_value_t *msg_props,
                                                        ten_error_t *err);

TEN_RUNTIME_PRIVATE_API const char *ten_msg_schema_get_msg_name(
    ten_msg_schema_t *self);
