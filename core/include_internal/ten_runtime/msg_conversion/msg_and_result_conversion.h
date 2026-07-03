//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/value/value.h"

typedef struct ten_msg_conversion_t ten_msg_conversion_t;

typedef struct ten_msg_and_result_conversion_t {
  ten_msg_conversion_t *msg;
  ten_msg_conversion_t *result;
} ten_msg_and_result_conversion_t;

TEN_RUNTIME_PRIVATE_API void ten_msg_and_result_conversion_destroy(
    ten_msg_and_result_conversion_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_and_result_conversion_t *
ten_msg_and_result_conversion_from_json(ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_msg_and_result_conversion_to_json(
    ten_msg_and_result_conversion_t *self, ten_json_t *json, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_msg_and_result_conversion_t *
ten_msg_and_result_conversion_from_value(ten_value_t *value, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_msg_and_result_conversion_to_value(
    ten_msg_and_result_conversion_t *self, ten_error_t *err);
