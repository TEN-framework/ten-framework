//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ten_utils/lib/error.h"

typedef struct ten_value_t ten_value_t;
typedef struct ten_buf_t ten_buf_t;
typedef struct ten_error_t ten_error_t;

// Magic number for validation
#define TEN_VALUE_BUFFER_MAGIC 0x10E

// Type tags used in buffer format
typedef enum TEN_VALUE_BUFFER_TYPE {
  TEN_VALUE_BUFFER_TYPE_INVALID = 0,

  TEN_VALUE_BUFFER_TYPE_BOOL = 1,

  TEN_VALUE_BUFFER_TYPE_INT8 = 2,
  TEN_VALUE_BUFFER_TYPE_INT16 = 3,
  TEN_VALUE_BUFFER_TYPE_INT32 = 4,
  TEN_VALUE_BUFFER_TYPE_INT64 = 5,

  TEN_VALUE_BUFFER_TYPE_UINT8 = 6,
  TEN_VALUE_BUFFER_TYPE_UINT16 = 7,
  TEN_VALUE_BUFFER_TYPE_UINT32 = 8,
  TEN_VALUE_BUFFER_TYPE_UINT64 = 9,

  TEN_VALUE_BUFFER_TYPE_FLOAT32 = 10,
  TEN_VALUE_BUFFER_TYPE_FLOAT64 = 11,

  TEN_VALUE_BUFFER_TYPE_STRING = 12,
  TEN_VALUE_BUFFER_TYPE_BUF = 13,
  TEN_VALUE_BUFFER_TYPE_ARRAY = 14,
  TEN_VALUE_BUFFER_TYPE_OBJECT = 15,
  TEN_VALUE_BUFFER_TYPE_PTR = 16,

  TEN_VALUE_BUFFER_TYPE_JSON_STRING = 17,
} TEN_VALUE_BUFFER_TYPE;

// Buffer header structure for value serialization.
//
// Layout:
// [magic:2][version:1][type:1][size:4][data...]
typedef struct ten_value_buffer_header_t {
  uint16_t magic;   // Magic number for validation
  uint8_t version;  // Protocol version
  uint8_t type;     // Value type
  uint32_t size;    // Size of serialized data following this header
} ten_value_buffer_header_t;

TEN_UTILS_API uint8_t *ten_value_serialize_to_buffer_c(ten_value_t *value,
                                                       size_t *buffer_size,
                                                       ten_error_t *err);

TEN_UTILS_API ten_value_t *ten_value_deserialize_from_buffer_c(
    const uint8_t *buffer, size_t buffer_size, size_t *bytes_consumed,
    ten_error_t *err);
