//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

ten_value_t *ten_value_object_peek(ten_value_t *self, const char *key) {
  TEN_ASSERT(self && ten_value_check_integrity(self) && key,
             "Invalid argument.");

  if (!ten_value_is_object(self)) {
    TEN_ASSERT(0, "Invalid argument.");
    return NULL;
  }

  ten_list_foreach (&self->content.object, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Invalid argument.");

    if (ten_string_is_equal_c_str(&kv->key, key)) {
      return kv->value;
    }
  }

  return NULL;
}

bool ten_value_object_get_bool(ten_value_t *self, const char *key,
                               ten_error_t *err) {
  TEN_ASSERT(self && ten_value_check_integrity(self) && key,
             "Invalid argument.");

  ten_value_t *v = ten_value_object_peek(self, key);
  if (!v) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "%s does not exist.", key);
    }
    return false;
  }

  bool result = ten_value_get_bool(v, err);
  bool success = ten_error_is_success(err);

  if (success) {
    return result;
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to get boolean value from %s", key);
    }
    return false;
  }
}

const char *ten_value_object_peek_string(ten_value_t *self, const char *key) {
  TEN_ASSERT(self && ten_value_check_integrity(self) && key,
             "Invalid argument.");

  ten_value_t *v = ten_value_object_peek(self, key);
  if (!v) {
    return NULL;
  }

  if (!ten_value_is_string(v)) {
    return NULL;
  }

  const char *result = ten_value_peek_raw_str(v, NULL);
  return result;
}

ten_list_t *ten_value_object_peek_array(ten_value_t *self, const char *key) {
  TEN_ASSERT(self && ten_value_check_integrity(self) && key,
             "Invalid argument.");

  ten_value_t *v = ten_value_object_peek(self, key);
  if (!v) {
    return NULL;
  }

  if (!ten_value_is_array(v)) {
    return NULL;
  }

  return ten_value_peek_array(v);
}

bool ten_value_object_move(ten_value_t *self, const char *key,
                           ten_value_t *value) {
  if (!self || !key || !value) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  if (!ten_value_is_object(self)) {
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  bool found = false;

  ten_list_foreach (&self->content.object, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Invalid argument.");

    if (ten_string_is_equal_c_str(&kv->key, key)) {
      found = true;

      if (kv->value) {
        ten_value_destroy(kv->value);
        kv->value = value;
      }
    }
  }

  if (!found) {
    ten_value_kv_t *kv = ten_value_kv_create(key, value);
    TEN_ASSERT(kv, "Failed to create value_kv.");

    ten_list_push_ptr_back(
        &self->content.object, kv,
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  return true;
}
