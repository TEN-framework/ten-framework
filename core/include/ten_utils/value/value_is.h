//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/value/value_kv.h"

TEN_UTILS_API bool ten_value_is_object(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_array(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_string(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_invalid(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_int8(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_int16(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_int32(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_int64(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_uint8(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_uint16(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_uint32(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_uint64(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_float32(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_float64(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_number(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_null(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_bool(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_ptr(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_buf(ten_value_t *self);
