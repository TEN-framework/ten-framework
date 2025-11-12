//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

typedef struct ten_value_t ten_value_t;
typedef struct ten_error_t ten_error_t;

/**
 * @brief Convert the value to int8 if the value does not overflow, even though
 * the range of the value type is larger than int8. Note that only the integer
 * type (i.e., uint8/16/32/64, int8/16/32/64) can be converted to int8, the
 * accuracy of the value can not be lost.
 */
TEN_UTILS_API bool ten_value_convert_to_int8(ten_value_t *self,
                                             ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_int16(ten_value_t *self,
                                              ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_int32(ten_value_t *self,
                                              ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_int64(ten_value_t *self,
                                              ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_uint8(ten_value_t *self,
                                              ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_uint16(ten_value_t *self,
                                               ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_uint32(ten_value_t *self,
                                               ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_uint64(ten_value_t *self,
                                               ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_float32(ten_value_t *self,
                                                ten_error_t *err);

TEN_UTILS_API bool ten_value_convert_to_float64(ten_value_t *self,
                                                ten_error_t *err);
