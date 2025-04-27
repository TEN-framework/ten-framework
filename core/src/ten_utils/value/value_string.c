//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/value/value_string.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>

#include "include_internal/ten_utils/value/constant_str.h"
#include "include_internal/ten_utils/value/value_convert.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

static bool ten_value_array_to_string(ten_value_t *self, ten_string_t *str,
                                      ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(ten_value_is_array(self), "Invalid argument: %d",
             ten_value_get_type(self));
  TEN_ASSERT(str, "Invalid argument.");

  ten_string_append_formatted(str, "%s", "[");

  ten_list_foreach (&self->content.array, iter) {
    ten_value_t *item = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(item && ten_value_check_integrity(item), "Invalid argument.");

    if (iter.index > 0) {
      ten_string_append_formatted(str, "%s", ",");
    }

    ten_string_t item_str;
    TEN_STRING_INIT(item_str);
    if (!ten_value_to_string(item, &item_str, err)) {
      ten_string_deinit(&item_str);
      return false;
    }

    ten_string_append_formatted(str, "%s", ten_string_get_raw_str(&item_str));
    ten_string_deinit(&item_str);
  }

  ten_string_append_formatted(str, "%s", "]");

  return true;
}

static bool ten_value_object_to_string(ten_value_t *self, ten_string_t *str,
                                       ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(ten_value_is_object(self), "Invalid argument: %d",
             ten_value_get_type(self));
  TEN_ASSERT(str, "Invalid argument.");

  ten_string_append_formatted(str, "%s", "{");

  ten_list_foreach (&self->content.array, iter) {
    ten_value_kv_t *item = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(item && ten_value_kv_check_integrity(item), "Invalid argument.");

    if (iter.index > 0) {
      ten_string_append_formatted(str, "%s", ",");
    }

    ten_string_t *item_str = ten_value_kv_to_string(item, err);
    if (!item_str) {
      return false;
    }

    ten_string_append_formatted(str, "%s", item_str);
    ten_string_destroy(item_str);
  }

  ten_string_append_formatted(str, "%s", "}");

  return true;
}

bool ten_value_to_string(ten_value_t *self, ten_string_t *str,
                         ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(str, "Invalid argument.");

  switch (self->type) {
  case TEN_TYPE_INVALID:
    TEN_ASSERT(0, "Should not happen.");
    break;
  case TEN_TYPE_INT8:
    ten_string_append_formatted(str, "%d", self->content.int8);
    break;
  case TEN_TYPE_INT16:
    ten_string_append_formatted(str, "%d", self->content.int16);
    break;
  case TEN_TYPE_INT32:
    ten_string_append_formatted(str, "%d", self->content.int32);
    break;
  case TEN_TYPE_INT64:
    ten_string_append_formatted(str, "%ld", self->content.int64);
    break;
  case TEN_TYPE_UINT8:
    ten_string_append_formatted(str, "%u", self->content.uint8);
    break;
  case TEN_TYPE_UINT16:
    ten_string_append_formatted(str, "%u", self->content.uint16);
    break;
  case TEN_TYPE_UINT32:
    ten_string_append_formatted(str, "%u", self->content.uint32);
    break;
  case TEN_TYPE_UINT64:
    ten_string_append_formatted(str, "%lu", self->content.uint64);
    break;
  case TEN_TYPE_FLOAT32:
    ten_string_append_formatted(str, "%f", self->content.float32);
    break;
  case TEN_TYPE_FLOAT64:
    ten_string_append_formatted(str, "%f", self->content.float64);
    break;
  case TEN_TYPE_NULL:
    break;
  case TEN_TYPE_PTR:
    ten_string_append_formatted(str, "0x%" PRIXPTR, self->content.ptr);
    break;
  case TEN_TYPE_BUF:
    ten_string_append_formatted(str, "0x%" PRIXPTR, self->content.buf.data);
    break;
  case TEN_TYPE_BOOL:
    ten_string_append_formatted(
        str, "%s",
        ten_value_get_bool(self, err) ? TEN_STR_TRUE : TEN_STR_FALSE);
    break;
  case TEN_TYPE_STRING:
    ten_string_append_formatted(str, "%s",
                                ten_string_get_raw_str(&self->content.string));
    break;
  case TEN_TYPE_ARRAY:
    if (!ten_value_array_to_string(self, str, err)) {
      return false;
    }
    break;
  case TEN_TYPE_OBJECT:
    if (!ten_value_object_to_string(self, str, err)) {
      return false;
    }
    break;
  default:
    TEN_ASSERT(0, "Need to implement more.");
    break;
  }

  return true;
}

static bool ten_value_adjust_from_int_type(TEN_TYPE type, ten_value_t *result,
                                           ten_error_t *err) {
  TEN_ASSERT(result && err, "Invalid argument.");

  bool success = false;

  switch (type) {
  case TEN_TYPE_INT8:
    success = ten_value_convert_to_int8(result, err);
    break;
  case TEN_TYPE_INT16:
    success = ten_value_convert_to_int16(result, err);
    break;
  case TEN_TYPE_INT32:
    success = ten_value_convert_to_int32(result, err);
    break;
  case TEN_TYPE_INT64:
    break;
  case TEN_TYPE_UINT8:
    success = ten_value_convert_to_uint8(result, err);
    break;
  case TEN_TYPE_UINT16:
    success = ten_value_convert_to_uint16(result, err);
    break;
  case TEN_TYPE_UINT32:
    success = ten_value_convert_to_uint32(result, err);
    break;
  case TEN_TYPE_UINT64:
    success = ten_value_convert_to_uint64(result, err);
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return success;
}

ten_value_t *ten_value_from_type_and_string(TEN_TYPE type, const char *str,
                                            ten_error_t *err) {
  TEN_ASSERT(type, "Invalid argument.");
  TEN_ASSERT(str, "Invalid argument.");

  bool success = true;
  ten_value_t *result = NULL;

  switch (type) {
  case TEN_TYPE_INT8:
  case TEN_TYPE_INT16:
  case TEN_TYPE_INT32:
  case TEN_TYPE_INT64: {
    errno = 0;
    int64_t int64_val = strtol(str, NULL, 10);
    if (errno == ERANGE) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Integer value out of range");
      }
      return NULL;
    }

    result = ten_value_create_int64(int64_val);
    success = ten_value_adjust_from_int_type(type, result, err);
    break;
  }

  case TEN_TYPE_UINT8:
  case TEN_TYPE_UINT16:
  case TEN_TYPE_UINT32:
  case TEN_TYPE_UINT64: {
    errno = 0;
    uint64_t uint64_val = strtoul(str, NULL, 10);
    if (errno == ERANGE) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Integer value out of range");
      }
      return NULL;
    }

    result = ten_value_create_uint64(uint64_val);
    success = ten_value_adjust_from_int_type(type, result, err);
    break;
  }

  case TEN_TYPE_STRING:
    result = ten_value_create_string(str);
    break;

  case TEN_TYPE_BOOL:
    result = ten_value_create_bool(
        ten_c_string_is_equal(str, TEN_STR_TRUE) ? true : false);
    break;

  case TEN_TYPE_NULL:
    result = ten_value_create_null();
    break;

  case TEN_TYPE_FLOAT32:
  case TEN_TYPE_FLOAT64: {
    errno = 0;
    double double_val = strtod(str, NULL);
    if (errno == ERANGE) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Floating point value out of range");
      }
      return NULL;
    }

    switch (type) {
    case TEN_TYPE_FLOAT32:
      result = ten_value_create_float32(
          double_val < -FLT_MAX || double_val > FLT_MAX ? 0.0F
                                                        : (float)double_val);
      break;
    case TEN_TYPE_FLOAT64:
      result = ten_value_create_float64(
          double_val < -DBL_MAX || double_val > DBL_MAX ? 0.0 : double_val);
      break;
    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Need to implement more operators.");
    break;
  }

  if (!success) {
    if (result) {
      ten_value_destroy(result);
    }
    result = NULL;
  }

  return result;
}
