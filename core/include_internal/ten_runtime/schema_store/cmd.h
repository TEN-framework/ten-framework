//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/schema_store/msg.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/value/value.h"

#define TEN_CMD_SCHEMA_SIGNATURE 0x740A46778CEC4CE8U

typedef struct ten_schema_t ten_schema_t;

// The schema definitions are as follows:
//
// "api": {
//   "cmd_in": [
//     {                         <== This section will be passed to
//                                   `ten_cmd_schema_create`.
//       "name": "cmd_foo",
//       "property": {           <== Stored in `hdr`.
//         "foo": {
//           "type": "string"
//         }
//       },
//       "result": {
//         "property": {         <== Stored in `cmd_result_schema`.
//           "status_foo": {
//             "type": "uint8"
//           }
//         }
//       }
//     }
//   ]
// }
typedef struct ten_cmd_schema_t {
  // This field must be the first field, as the `hdr.hh_in_map` will be used to
  // locate the address of the `ten_cmd_schema_t`.
  ten_msg_schema_t hdr;

  ten_signature_t signature;

  ten_schema_t *cmd_result_schema;
} ten_cmd_schema_t;

TEN_RUNTIME_PRIVATE_API bool ten_cmd_schema_check_integrity(
    ten_cmd_schema_t *self);

TEN_RUNTIME_PRIVATE_API ten_cmd_schema_t *ten_cmd_schema_create(
    ten_value_t *cmd_schema);

TEN_RUNTIME_PRIVATE_API void ten_cmd_schema_destroy(ten_cmd_schema_t *self);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_cmd_schema_get_cmd_name(
    ten_cmd_schema_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_cmd_schema_validate_cmd_result_properties(
    ten_cmd_schema_t *self, ten_value_t *status_props, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_cmd_schema_adjust_cmd_result_properties(
    ten_cmd_schema_t *self, ten_value_t *status_props, ten_error_t *err);
