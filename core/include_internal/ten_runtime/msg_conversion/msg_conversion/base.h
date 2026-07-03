//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/value/value.h"

typedef struct ten_msg_conversion_t ten_msg_conversion_t;

typedef ten_shared_ptr_t *(*ten_msg_conversion_func_t)(
    ten_msg_conversion_t *operation, ten_shared_ptr_t *msg, ten_error_t *err);

/**
 * If the desired message conversion is beyond the capabilities of the TEN
 * runtime, an extension can be used to handle the conversion. To avoid having
 * message conversion logic intrude into other extensions, a dedicated extension
 * specifically for message conversion can be implemented. This standalone
 * extension should be designed with enough flexibility to accommodate various
 * conversion needs.
 */
typedef enum TEN_MSG_CONVERSION_TYPE {
  TEN_MSG_CONVERSION_TYPE_INVALID,

  TEN_MSG_CONVERSION_TYPE_PER_PROPERTY,
} TEN_MSG_CONVERSION_TYPE;

typedef struct ten_msg_conversion_t {
  TEN_MSG_CONVERSION_TYPE type;
  ten_msg_conversion_func_t operation;
} ten_msg_conversion_t;

TEN_RUNTIME_PRIVATE_API void ten_msg_conversion_destroy(
    ten_msg_conversion_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_t *ten_msg_conversion_from_json(
    ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_conversion_to_json(
    ten_msg_conversion_t *self, ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_conversion_t *ten_msg_conversion_from_value(
    ten_value_t *value, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_msg_conversion_to_value(
    ten_msg_conversion_t *self, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_msg_conversion_convert(
    ten_msg_conversion_t *self, ten_shared_ptr_t *msg, ten_error_t *err);
