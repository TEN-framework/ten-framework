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
#include "ten_utils/value/type.h"

#define TEN_JSON_SIGNATURE 0xACA499C637670350U

#define TEN_JSON_INIT_VAL(ctx_, owned_ctx_)                     \
  (ten_json_t) {                                                \
    .signature = TEN_JSON_SIGNATURE, .json = NULL, .ctx = ctx_, \
    .owned_ctx = owned_ctx_,                                    \
  }

#define ten_json_object_foreach(object, key, value)                    \
  ten_json_iter_t __iter;                                              \
  ten_json_object_iter_init(object, &__iter);                          \
  ten_json_t __key = TEN_JSON_INIT_VAL((object)->ctx, false);          \
  ten_json_t __value = TEN_JSON_INIT_VAL((object)->ctx, false);        \
  key = ten_json_object_iter_next_key(&__iter, &__key);                \
  for (; (key) && ten_json_object_iter_peek_value(&__key, &__value) && \
         ((value) = &__value);                                         \
       (key) = ten_json_object_iter_next_key(&__iter, &__key))

#define ten_json_array_foreach(array, index, item)                          \
  ten_json_t __item = TEN_JSON_INIT_VAL(array->ctx, false);                 \
  for ((index) = 0;                                                         \
       (index) < ten_json_array_get_size(array) &&                          \
       ten_json_array_peek_item(array, index, &__item) && (item = &__item); \
       (index)++)

// Since the `yyjson` iterator is not a pointer but a complete struct, to avoid
// requiring files outside of `json.c` to include the `yyjson` header file, a
// trick is used here. A memory space larger than the `yyjson` iterator struct
// is allocated, allowing external users to interact with this struct without
// directly involving `yyjson`. Additionally, in `json.c`, a `static_assert` is
// used to ensure that the allocated size is indeed larger than the `yyjson`
// iterator.
typedef struct ten_json_iter_t {
  char payload[64];
} ten_json_iter_t;

typedef struct ten_json_t {
  ten_signature_t signature;

  void *json;  // yyjson_mut_val *
  void *ctx;   // yyjson_mut_doc *
  bool owned_ctx;
} ten_json_t;

TEN_UTILS_API bool ten_json_check_integrity(ten_json_t *self);

TEN_UTILS_API void ten_json_init(ten_json_t *self, void *ctx, bool owned_ctx);

TEN_UTILS_API bool ten_json_init_object(ten_json_t *self);

TEN_UTILS_API bool ten_json_init_array(ten_json_t *self);

TEN_UTILS_API bool ten_json_init_string(ten_json_t *self, const char *value);

TEN_UTILS_API bool ten_json_init_integer(ten_json_t *self, int64_t value);

TEN_UTILS_API bool ten_json_init_real(ten_json_t *self, double value);

TEN_UTILS_API bool ten_json_init_boolean(ten_json_t *self, bool value);

TEN_UTILS_API bool ten_json_init_null(ten_json_t *self);

TEN_UTILS_API void ten_json_deinit(ten_json_t *self);

TEN_UTILS_API ten_json_t *ten_json_create(void *ctx, bool owned_ctx);

TEN_UTILS_API void ten_json_destroy(ten_json_t *self);

TEN_UTILS_API TEN_TYPE ten_json_get_type(ten_json_t *self);

TEN_UTILS_API bool ten_json_object_peek(ten_json_t *self, const char *key,
                                        ten_json_t *value);

TEN_UTILS_API const char *ten_json_object_peek_string(ten_json_t *self,
                                                      const char *key);

TEN_UTILS_API bool ten_json_object_peek_or_create_object(ten_json_t *self,
                                                         const char *key,
                                                         ten_json_t *object);

TEN_UTILS_API bool ten_json_object_peek_or_create_array(ten_json_t *self,
                                                        const char *key,
                                                        ten_json_t *array);

TEN_UTILS_API bool ten_json_object_set(ten_json_t *self, const char *key,
                                       ten_json_t *value);

TEN_UTILS_API bool ten_json_object_set_string(ten_json_t *self, const char *key,
                                              const char *value);

TEN_UTILS_API bool ten_json_object_set_int(ten_json_t *self, const char *key,
                                           int64_t value);

TEN_UTILS_API bool ten_json_object_set_real(ten_json_t *self, const char *key,
                                            double value);

TEN_UTILS_API bool ten_json_object_set_bool(ten_json_t *self, const char *key,
                                            bool value);

TEN_UTILS_API bool ten_json_object_iter_init(ten_json_t *self,
                                             ten_json_iter_t *iter);

TEN_UTILS_API const char *ten_json_object_iter_next_key(void *iter,
                                                        ten_json_t *key);

TEN_UTILS_API bool ten_json_object_iter_peek_value(ten_json_t *key,
                                                   ten_json_t *value);

TEN_UTILS_API bool ten_json_array_append(ten_json_t *self, ten_json_t *item);

TEN_UTILS_API size_t ten_json_array_get_size(ten_json_t *self);

TEN_UTILS_API bool ten_json_array_peek_item(ten_json_t *self, size_t index,
                                            ten_json_t *item);

TEN_UTILS_API const char *ten_json_to_string(ten_json_t *self, const char *key,
                                             bool *must_free);

TEN_UTILS_API ten_json_t *ten_json_from_string(const char *value,
                                               ten_error_t *err);

TEN_UTILS_API bool ten_json_is_object(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_array(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_string(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_integer(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_boolean(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_real(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_true(ten_json_t *self);

TEN_UTILS_API bool ten_json_is_null(ten_json_t *self);

TEN_UTILS_API const char *ten_json_peek_string_value(ten_json_t *self);

TEN_UTILS_API int64_t ten_json_get_integer_value(ten_json_t *self);

TEN_UTILS_API bool ten_json_get_boolean_value(ten_json_t *self);

TEN_UTILS_API double ten_json_get_real_value(ten_json_t *self);

TEN_UTILS_API ten_json_t *ten_json_create_root_object(void);

TEN_UTILS_API void *ten_json_create_new_ctx(void);
