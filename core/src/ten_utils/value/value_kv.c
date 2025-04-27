//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/value/value_kv.h"

#include <stdarg.h>
#include <stdlib.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

bool ten_value_kv_check_integrity(ten_value_kv_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_VALUE_KV_SIGNATURE) {
    return false;
  }
  return true;
}

ten_value_kv_t *ten_value_kv_create_empty(const char *name) {
  TEN_ASSERT(name, "Invalid argument.");
  return ten_value_kv_create_vempty(name);
}

ten_value_kv_t *ten_value_kv_create_vempty(const char *fmt, ...) {
  TEN_ASSERT(fmt, "Invalid argument.");

  ten_value_kv_t *self = (ten_value_kv_t *)ten_malloc(sizeof(ten_value_kv_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_VALUE_KV_SIGNATURE);

  TEN_STRING_INIT(self->key);

  va_list ap;
  va_start(ap, fmt);
  ten_string_append_from_va_list(&self->key, fmt, ap);
  va_end(ap);

  self->value = NULL;

  return self;
}

/**
 * @note Note that the ownership of @a value is moved to the value_kv.
 */
ten_value_kv_t *ten_value_kv_create(const char *name, ten_value_t *value) {
  TEN_ASSERT(name && value && ten_value_check_integrity(value),
             "Invalid argument.");

  ten_value_kv_t *self = ten_value_kv_create_empty(name);
  self->value = value;

  return self;
}

ten_string_t *ten_value_kv_get_key(ten_value_kv_t *self) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");
  return &self->key;
}

ten_value_t *ten_value_kv_get_value(ten_value_kv_t *self) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");
  return self->value;
}

/**
 * @note Note that the ownership of @a value is moved to the value_kv.
 */
void ten_value_kv_reset_to_value(ten_value_kv_t *self, ten_value_t *value) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");

  if (!self) {
    return;
  }

  if (self->value) {
    ten_value_destroy(self->value);
  }
  self->value = value;
}

void ten_value_kv_destroy(ten_value_kv_t *self) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");

  ten_string_deinit(&self->key);
  if (self->value) {
    ten_value_destroy(self->value);
  }
  ten_free(self);
}

void ten_value_kv_destroy_key_only(ten_value_kv_t *self) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");

  ten_string_deinit(&self->key);
  self->value = NULL;

  ten_free(self);
}

ten_value_kv_t *ten_value_kv_clone(ten_value_kv_t *target) {
  TEN_ASSERT(target && ten_value_kv_check_integrity(target),
             "Invalid argument.");

  ten_value_kv_t *self =
      ten_value_kv_create_empty(ten_string_get_raw_str(&target->key));

  if (target->value) {
    self->value = ten_value_clone(target->value);
  }

  return self;
}

ten_string_t *ten_value_kv_to_string(ten_value_kv_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_value_kv_check_integrity(self), "Invalid argument.");

  ten_string_t *result =
      ten_string_create_formatted("%s:", ten_string_get_raw_str(&self->key));
  TEN_ASSERT(result, "Invalid argument.");

  ten_string_t value_str;
  TEN_STRING_INIT(value_str);

  if (!ten_value_to_string(self->value, &value_str, err)) {
    ten_string_destroy(result);
    ten_string_deinit(&value_str);
    return NULL;
  }

  ten_string_append_formatted(result, "%s", ten_string_get_raw_str(&value_str));

  ten_string_deinit(&value_str);

  return result;
}

ten_value_kv_t *ten_value_kv_from_json(const char *key, ten_json_t *json) {
  TEN_ASSERT(key, "Invalid argument.");
  TEN_ASSERT(json, "Invalid argument.");

  ten_value_kv_t *kv = ten_value_kv_create_empty(key);
  TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Invalid argument.");

  kv->value = ten_value_from_json(json);
  TEN_ASSERT(kv->value && ten_value_check_integrity(kv->value),
             "Invalid argument.");

  return kv;
}

bool ten_value_kv_to_json(ten_value_kv_t *self, ten_json_t *json) {
  TEN_ASSERT(json, "Invalid argument.");
  TEN_ASSERT(self, "Invalid argument.");

  ten_json_t value_json = TEN_JSON_INIT_VAL(json->ctx, false);
  bool success = ten_value_to_json(self->value, &value_json);
  TEN_ASSERT(success, "Should not happen.");

  return ten_json_object_set(json, ten_string_get_raw_str(&self->key),
                             &value_json);
}
