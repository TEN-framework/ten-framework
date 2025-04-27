//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "stdint.h"

typedef struct ten_waitable_object_t ten_waitable_number_t;

TEN_UTILS_API ten_waitable_number_t *ten_waitable_number_create(
    int64_t init_value);

TEN_UTILS_API void ten_waitable_number_destroy(ten_waitable_number_t *number);

TEN_UTILS_API void ten_waitable_number_increase(ten_waitable_number_t *number,
                                                int64_t value);

TEN_UTILS_API void ten_waitable_number_decrease(ten_waitable_number_t *number,
                                                int64_t value);

TEN_UTILS_API void ten_waitable_number_multiply(ten_waitable_number_t *number,
                                                int64_t value);

TEN_UTILS_API void ten_waitable_number_divide(ten_waitable_number_t *number,
                                              int64_t value);

TEN_UTILS_API void ten_waitable_number_set(ten_waitable_number_t *number,
                                           int64_t value);

TEN_UTILS_API int64_t ten_waitable_number_get(ten_waitable_number_t *number);

TEN_UTILS_API int ten_waitable_number_wait_until(ten_waitable_number_t *number,
                                                 int64_t value, int timeout);

TEN_UTILS_API int ten_waitable_number_wait_while(ten_waitable_number_t *number,
                                                 int64_t value, int timeout);
