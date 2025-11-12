//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/container/hash_handle.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/value/value.h"

#define TEN_SCHEMA_KEYWORD_SIGNATURE 0x94F75C7B1835B931U

typedef struct ten_schema_t ten_schema_t;
typedef struct ten_schema_keyword_t ten_schema_keyword_t;
typedef struct ten_schema_error_t ten_schema_error_t;

typedef void (*ten_schema_keyword_destroy_func_t)(ten_schema_keyword_t *self);

typedef bool (*ten_schema_keyword_validate_value_func_t)(
    ten_schema_keyword_t *self, ten_value_t *value,
    ten_schema_error_t *schema_err);

typedef bool (*ten_schema_keyword_adjust_value_func_t)(
    ten_schema_keyword_t *self, ten_value_t *value,
    ten_schema_error_t *schema_err);

/**
 * @brief Check if the keyword is compatible with the target keyword. Note that
 * the `self` or `target` might be NULL (i.e., the keyword is not defined in the
 * schema), all implementations should handle this case properly.
 */
typedef bool (*ten_schema_keyword_is_compatible_func_t)(
    ten_schema_keyword_t *self, ten_schema_keyword_t *target,
    ten_schema_error_t *schema_err);

typedef enum TEN_SCHEMA_KEYWORD {
  TEN_SCHEMA_KEYWORD_INVALID = 0,

  TEN_SCHEMA_KEYWORD_TYPE,
  TEN_SCHEMA_KEYWORD_PROPERTIES,
  TEN_SCHEMA_KEYWORD_ITEMS,
  TEN_SCHEMA_KEYWORD_REQUIRED,

  // It's just used to determine the range of keyword type.
  TEN_SCHEMA_KEYWORD_LAST,
} TEN_SCHEMA_KEYWORD;

typedef struct ten_schema_keyword_t {
  ten_signature_t signature;

  TEN_SCHEMA_KEYWORD type;
  ten_hashhandle_t hh_in_keyword_map;

  ten_schema_t *owner;
  ten_schema_keyword_destroy_func_t destroy;
  ten_schema_keyword_validate_value_func_t validate_value;
  ten_schema_keyword_adjust_value_func_t adjust_value;
  ten_schema_keyword_is_compatible_func_t is_compatible;
} ten_schema_keyword_t;

TEN_UTILS_PRIVATE_API bool ten_schema_keyword_check_integrity(
    ten_schema_keyword_t *self);

TEN_UTILS_PRIVATE_API void ten_schema_keyword_init(ten_schema_keyword_t *self,
                                                   TEN_SCHEMA_KEYWORD type);

TEN_UTILS_PRIVATE_API void ten_schema_keyword_deinit(
    ten_schema_keyword_t *self);
