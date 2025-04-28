//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"

typedef enum TEN_VALUE_PATH_ITEM_TYPE {
  TEN_VALUE_PATH_ITEM_TYPE_INVALID,

  TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM,
  TEN_VALUE_PATH_ITEM_TYPE_ARRAY_ITEM,
} TEN_VALUE_PATH_ITEM_TYPE;

typedef struct ten_value_path_item_t {
  TEN_VALUE_PATH_ITEM_TYPE type;

  union {
    ten_string_t obj_item_str;
    size_t arr_idx;
  };
} ten_value_path_item_t;

TEN_UTILS_API bool ten_value_path_parse(const char *path, ten_list_t *result,
                                        ten_error_t *err);

TEN_UTILS_API ten_value_t *ten_value_peek_from_path(ten_value_t *base,
                                                    const char *path,
                                                    ten_error_t *err);

TEN_UTILS_API bool ten_value_set_from_path_list_with_move(ten_value_t *base,
                                                          ten_list_t *paths,
                                                          ten_value_t *value,
                                                          ten_error_t *err);

TEN_UTILS_API bool ten_value_set_from_path_str_with_move(ten_value_t *base,
                                                         const char *path,
                                                         ten_value_t *value,
                                                         ten_error_t *err);
