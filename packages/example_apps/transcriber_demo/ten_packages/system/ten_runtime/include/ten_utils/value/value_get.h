//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "ten_utils/container/list.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value_kv.h"

TEN_UTILS_API TEN_TYPE ten_value_get_type(ten_value_t *self);

TEN_UTILS_API int8_t ten_value_get_int8(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API int16_t ten_value_get_int16(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API int32_t ten_value_get_int32(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API int64_t ten_value_get_int64(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API uint8_t ten_value_get_uint8(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API uint16_t ten_value_get_uint16(ten_value_t *self,
                                            ten_error_t *err);

TEN_UTILS_API uint32_t ten_value_get_uint32(ten_value_t *self,
                                            ten_error_t *err);

TEN_UTILS_API uint64_t ten_value_get_uint64(ten_value_t *self,
                                            ten_error_t *err);

TEN_UTILS_API float ten_value_get_float32(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API double ten_value_get_float64(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API bool ten_value_get_bool(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API ten_string_t *ten_value_peek_string(ten_value_t *self);

TEN_UTILS_API const char *ten_value_peek_raw_str(ten_value_t *self,
                                                 ten_error_t *err);

TEN_UTILS_API void *ten_value_get_ptr(ten_value_t *self, ten_error_t *err);

TEN_UTILS_API ten_buf_t *ten_value_peek_buf(ten_value_t *self,
                                            ten_error_t *err);

TEN_UTILS_API ten_list_t *ten_value_peek_array(ten_value_t *self);

TEN_UTILS_API ten_list_t *ten_value_peek_object(ten_value_t *self);

TEN_UTILS_API ten_value_t *ten_value_array_peek(ten_value_t *self, size_t index,
                                                ten_error_t *err);
