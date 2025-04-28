//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#include "include_internal/ten_utils/value/constant_str.h"
#include "ten_utils/value/type.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

#define TEN_IS_INTEGER_TYPE(t)                                               \
  ((t) == TEN_TYPE_INT8 || (t) == TEN_TYPE_UINT8 || (t) == TEN_TYPE_INT16 || \
   (t) == TEN_TYPE_UINT16 || (t) == TEN_TYPE_INT32 ||                        \
   (t) == TEN_TYPE_UINT32 || (t) == TEN_TYPE_INT64 || (t) == TEN_TYPE_UINT64)

#define TEN_IS_FLOAT_TYPE(t) \
  ((t) == TEN_TYPE_FLOAT32 || (t) == TEN_TYPE_FLOAT64)

typedef struct ten_type_info_t {
  const char *name;
} ten_type_info_t;

static const ten_type_info_t ten_types_info[] = {
    [TEN_TYPE_INVALID] =
        {
            .name = NULL,
        },
    [TEN_TYPE_NULL] =
        {
            .name = TEN_STR_NULL,
        },
    [TEN_TYPE_BOOL] =
        {
            .name = TEN_STR_BOOL,
        },
    [TEN_TYPE_INT8] =
        {
            .name = TEN_STR_INT8,
        },
    [TEN_TYPE_INT16] =
        {
            .name = TEN_STR_INT16,
        },
    [TEN_TYPE_INT32] =
        {
            .name = TEN_STR_INT32,
        },
    [TEN_TYPE_INT64] =
        {
            .name = TEN_STR_INT64,
        },
    [TEN_TYPE_UINT8] =
        {
            .name = TEN_STR_UINT8,
        },
    [TEN_TYPE_UINT16] =
        {
            .name = TEN_STR_UINT16,
        },
    [TEN_TYPE_UINT32] =
        {
            .name = TEN_STR_UINT32,
        },
    [TEN_TYPE_UINT64] =
        {
            .name = TEN_STR_UINT64,
        },
    [TEN_TYPE_FLOAT32] =
        {
            .name = TEN_STR_FLOAT32,
        },
    [TEN_TYPE_FLOAT64] =
        {
            .name = TEN_STR_FLOAT64,
        },
    [TEN_TYPE_ARRAY] =
        {
            .name = TEN_STR_ARRAY,
        },
    [TEN_TYPE_OBJECT] =
        {
            .name = TEN_STR_OBJECT,
        },
    [TEN_TYPE_PTR] =
        {
            .name = TEN_STR_PTR,
        },
    [TEN_TYPE_STRING] =
        {
            .name = TEN_STR_STRING,
        },
    [TEN_TYPE_BUF] =
        {
            .name = TEN_STR_BUF,
        },
};

static const size_t ten_types_info_size =
    sizeof(ten_types_info) / sizeof(ten_types_info[0]);
