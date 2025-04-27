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
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value_get.h"     // IWYU pragma: keep
#include "ten_utils/value/value_is.h"      // IWYU pragma: keep
#include "ten_utils/value/value_json.h"    // IWYU pragma: keep
#include "ten_utils/value/value_kv.h"      // IWYU pragma: keep
#include "ten_utils/value/value_merge.h"   // IWYU pragma: keep
#include "ten_utils/value/value_object.h"  // IWYU pragma: keep
#include "ten_utils/value/value_string.h"  // IWYU pragma: keep

typedef enum TEN_VALUE_ERROR {
  TEN_VALUE_ERROR_UNSUPPORTED_TYPE_CONVERSION = 1,
} TEN_VALUE_ERROR;

#define ten_value_object_foreach(value, iter) \
  ten_list_foreach (&((value)->content.object), iter)

#define ten_value_array_foreach(value, iter) \
  ten_list_foreach (&((value)->content.array), iter)

typedef struct ten_value_t ten_value_t;

typedef union ten_value_union_t {
  bool boolean;

  int8_t int8;
  int16_t int16;
  int32_t int32;
  int64_t int64;

  uint8_t uint8;
  uint16_t uint16;
  uint32_t uint32;
  uint64_t uint64;

  float float32;
  double float64;

  void *ptr;

  ten_list_t object;  // The element type is 'ten_value_kv_t*'
  ten_list_t array;   // The element type is 'ten_value_t*'
  ten_string_t string;
  ten_buf_t buf;
} ten_value_union_t;

typedef bool (*ten_value_construct_func_t)(ten_value_t *value,
                                           ten_error_t *err);

typedef bool (*ten_value_copy_func_t)(ten_value_t *dest, ten_value_t *src,
                                      ten_error_t *err);

typedef bool (*ten_value_destruct_func_t)(ten_value_t *value, ten_error_t *err);

typedef struct ten_value_t {
  ten_signature_t signature;

  /**
   * @brief The name of the value. Mainly for debug purpose.
   */
  ten_string_t *name;

  TEN_TYPE type;
  ten_value_union_t content;

  ten_value_construct_func_t construct;
  ten_value_copy_func_t copy;
  ten_value_destruct_func_t destruct;
} ten_value_t;

TEN_UTILS_API bool ten_value_check_integrity(ten_value_t *self);

TEN_UTILS_API ten_value_t *ten_value_clone(ten_value_t *src);
TEN_UTILS_API bool ten_value_copy(ten_value_t *src, ten_value_t *dest);

TEN_UTILS_API bool ten_value_init_invalid(ten_value_t *self);
TEN_UTILS_API bool ten_value_init_int8(ten_value_t *self, int8_t value);
TEN_UTILS_API bool ten_value_init_int16(ten_value_t *self, int16_t value);
TEN_UTILS_API bool ten_value_init_int32(ten_value_t *self, int32_t value);
TEN_UTILS_API bool ten_value_init_int64(ten_value_t *self, int64_t value);
TEN_UTILS_API bool ten_value_init_uint8(ten_value_t *self, uint8_t value);
TEN_UTILS_API bool ten_value_init_uint16(ten_value_t *self, uint16_t value);
TEN_UTILS_API bool ten_value_init_uint32(ten_value_t *self, uint32_t value);
TEN_UTILS_API bool ten_value_init_uint64(ten_value_t *self, uint64_t value);
TEN_UTILS_API bool ten_value_init_float32(ten_value_t *self, float value);
TEN_UTILS_API bool ten_value_init_float64(ten_value_t *self, double value);
TEN_UTILS_API bool ten_value_init_bool(ten_value_t *self, bool value);
TEN_UTILS_API bool ten_value_init_null(ten_value_t *self);
TEN_UTILS_API bool ten_value_init_string(ten_value_t *self);
TEN_UTILS_API bool ten_value_init_string_with_size(ten_value_t *self,
                                                   const char *str, size_t len);
TEN_UTILS_API bool ten_value_init_buf(ten_value_t *self, size_t size);

/**
 * @note Note that the ownership of @a value is moved to the value @a self.
 */
TEN_UTILS_API bool ten_value_init_object_with_move(ten_value_t *self,
                                                   ten_list_t *value);

/**
 * @note Note that the ownership of @a value is moved to the value @a self.
 */
TEN_UTILS_API bool ten_value_init_array_with_move(ten_value_t *self,
                                                  ten_list_t *value);

TEN_UTILS_API ten_value_t *ten_value_create_invalid(void);
TEN_UTILS_API ten_value_t *ten_value_create_int8(int8_t value);
TEN_UTILS_API ten_value_t *ten_value_create_int16(int16_t value);
TEN_UTILS_API ten_value_t *ten_value_create_int32(int32_t value);
TEN_UTILS_API ten_value_t *ten_value_create_int64(int64_t value);
TEN_UTILS_API ten_value_t *ten_value_create_uint8(uint8_t value);
TEN_UTILS_API ten_value_t *ten_value_create_uint16(uint16_t value);
TEN_UTILS_API ten_value_t *ten_value_create_uint32(uint32_t value);
TEN_UTILS_API ten_value_t *ten_value_create_uint64(uint64_t value);
TEN_UTILS_API ten_value_t *ten_value_create_float32(float value);
TEN_UTILS_API ten_value_t *ten_value_create_float64(double value);
TEN_UTILS_API ten_value_t *ten_value_create_bool(bool value);
TEN_UTILS_API ten_value_t *ten_value_create_array_with_move(ten_list_t *value);
TEN_UTILS_API ten_value_t *ten_value_create_object_with_move(ten_list_t *value);
TEN_UTILS_API ten_value_t *ten_value_create_string_with_size(const char *str,
                                                             size_t len);
TEN_UTILS_API ten_value_t *ten_value_create_string(const char *str);
TEN_UTILS_API ten_value_t *ten_value_create_null(void);
TEN_UTILS_API ten_value_t *ten_value_create_ptr(
    void *ptr, ten_value_construct_func_t construct, ten_value_copy_func_t copy,
    ten_value_destruct_func_t destruct);
TEN_UTILS_API ten_value_t *ten_value_create_buf_with_move(ten_buf_t buf);

TEN_UTILS_API void ten_value_deinit(ten_value_t *self);

TEN_UTILS_API void ten_value_destroy(ten_value_t *self);

TEN_UTILS_API void ten_value_reset_to_string_with_size(ten_value_t *self,
                                                       const char *str,
                                                       size_t len);

TEN_UTILS_API void ten_value_reset_to_ptr(ten_value_t *self, void *ptr,
                                          ten_value_construct_func_t construct,
                                          ten_value_copy_func_t copy,
                                          ten_value_destruct_func_t destruct);

TEN_UTILS_API void ten_value_set_name(ten_value_t *self, const char *fmt, ...);

TEN_UTILS_API size_t ten_value_array_size(ten_value_t *self);

TEN_UTILS_API bool ten_value_is_valid(ten_value_t *self);
