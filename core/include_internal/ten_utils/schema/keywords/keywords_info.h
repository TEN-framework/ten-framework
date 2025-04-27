//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "include_internal/ten_utils/schema/constant_str.h"
#include "include_internal/ten_utils/schema/keywords/keyword.h"
#include "include_internal/ten_utils/schema/keywords/keyword_items.h"
#include "include_internal/ten_utils/schema/keywords/keyword_properties.h"
#include "include_internal/ten_utils/schema/keywords/keyword_required.h"
#include "include_internal/ten_utils/schema/keywords/keyword_type.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

typedef ten_schema_keyword_t *(*ten_schema_keyword_create_from_value_func_t)(
    ten_schema_t *self, ten_value_t *value);

typedef struct ten_schema_keyword_info_t {
  const char *name;
  ten_schema_keyword_create_from_value_func_t from_value;
} ten_schema_keyword_info_t;

static const ten_schema_keyword_info_t ten_schema_keywords_info[] = {
    [TEN_SCHEMA_KEYWORD_TYPE] =
        {
            .name = TEN_SCHEMA_KEYWORD_STR_TYPE,
            .from_value = ten_schema_keyword_type_create_from_value,
        },
    [TEN_SCHEMA_KEYWORD_PROPERTIES] =
        {
            .name = TEN_SCHEMA_KEYWORD_STR_PROPERTIES,
            .from_value = ten_schema_keyword_properties_create_from_value,
        },
    [TEN_SCHEMA_KEYWORD_ITEMS] =
        {
            .name = TEN_SCHEMA_KEYWORD_STR_ITEMS,
            .from_value = ten_schema_keyword_items_create_from_value,
        },
    [TEN_SCHEMA_KEYWORD_REQUIRED] =
        {
            .name = TEN_SCHEMA_KEYWORD_STR_REQUIRED,
            .from_value = ten_schema_keyword_required_create_from_value,
        },
};

static const size_t ten_schema_keywords_info_size =
    sizeof(ten_schema_keywords_info) / sizeof(ten_schema_keywords_info[0]);

static inline const ten_schema_keyword_info_t *
ten_schema_keyword_info_get_by_name(const char *name) {
  for (size_t i = 0; i < ten_schema_keywords_info_size; i++) {
    if (ten_schema_keywords_info[i].name &&
        ten_c_string_is_equal(ten_schema_keywords_info[i].name, name)) {
      return &ten_schema_keywords_info[i];
    }
  }

  return NULL;
}

static inline const char *ten_schema_keyword_to_string(
    TEN_SCHEMA_KEYWORD keyword) {
  if (keyword == TEN_SCHEMA_KEYWORD_INVALID ||
      keyword == TEN_SCHEMA_KEYWORD_LAST) {
    TEN_ASSERT(0, "Invalid argument.");
    return NULL;
  }

  return ten_schema_keywords_info[keyword].name;
}
