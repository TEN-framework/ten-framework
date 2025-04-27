//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/value/type.h"

#include <float.h>
#include <stddef.h>

#include "include_internal/ten_utils/value/type_info.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

TEN_TYPE ten_type_from_string(const char *type_str) {
  TEN_ASSERT(type_str, "Invalid argument.");

  for (size_t i = 0; i < ten_types_info_size; ++i) {
    if (ten_types_info[i].name &&
        ten_c_string_is_equal(type_str, ten_types_info[i].name)) {
      return (TEN_TYPE)i;
    }
  }

  return TEN_TYPE_INVALID;
}

const char *ten_type_to_string(const TEN_TYPE type) {
  return ten_types_info[type].name;
}

ten_list_t ten_type_from_json(ten_json_t *json) {
  TEN_ASSERT(json, "Invalid argument.");

  ten_list_t result = TEN_LIST_INIT_VAL;

  static_assert(sizeof(TEN_TYPE) <= sizeof(int32_t),
                "TEN_TYPE can not be larger than 32-bit.");

  if (ten_json_is_integer(json)) {
    if (ten_json_get_integer_value(json) >= INT32_MIN &&
        ten_json_get_integer_value(json) <= INT32_MAX) {
      ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_INT32));
    }
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_INT64));
  } else if (ten_json_is_real(json)) {
    if (ten_json_get_real_value(json) >= -FLT_MAX &&
        ten_json_get_real_value(json) <= FLT_MAX) {
      ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_FLOAT32));
    }
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_FLOAT64));
  } else if (ten_json_is_null(json)) {
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_NULL));
  } else if (ten_json_is_string(json)) {
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_STRING));
  } else if (ten_json_is_object(json)) {
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_OBJECT));
  } else if (ten_json_is_array(json)) {
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_ARRAY));
  } else if (ten_json_is_boolean(json)) {
    ten_list_push_back(&result, ten_int32_listnode_create(TEN_TYPE_BOOL));
  } else {
    TEN_ASSERT(0, "Handle more types.");
  }

  return result;
}

// When converting `ten_value_t` between different types, this function is used
// to check if the types before and after the conversion are compatible. If they
// are not compatible, the conversion will not proceed. This mechanism ensures
// that compatibility between `TEN` types throughout the entire `TEN` system is
// managed consistently by this function. Even in `TEN` Rust code, if type
// compatibility between `TEN` types needs to be checked, it will ultimately
// rely on this function.
bool ten_type_is_compatible(TEN_TYPE actual, TEN_TYPE expected) {
  if (actual == TEN_TYPE_INVALID || expected == TEN_TYPE_INVALID) {
    return false;
  }

  if (actual == expected) {
    return true;
  }

  if (TEN_IS_INTEGER_TYPE(expected)) {
    return TEN_IS_INTEGER_TYPE(actual);
  }

  if (TEN_IS_FLOAT_TYPE(expected)) {
    return TEN_IS_INTEGER_TYPE(actual) || TEN_IS_FLOAT_TYPE(actual);
  }

  switch (expected) {
  case TEN_TYPE_BOOL:
    return actual == TEN_TYPE_BOOL;

  case TEN_TYPE_PTR:
    return actual == TEN_TYPE_PTR;

  case TEN_TYPE_STRING:
    return actual == TEN_TYPE_STRING;

  case TEN_TYPE_ARRAY:
    return actual == TEN_TYPE_ARRAY;

  case TEN_TYPE_OBJECT:
    return actual == TEN_TYPE_OBJECT;

  case TEN_TYPE_BUF:
    return actual == TEN_TYPE_BUF;

  case TEN_TYPE_NULL:
    return actual == TEN_TYPE_NULL;

  default:
    return false;
  }

  TEN_ASSERT(0, "Should not happen.");
}
