//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

typedef struct ten_value_t ten_value_t;

typedef struct ten_value_kv_t {
  ten_signature_t signature;

  ten_string_t key;
  ten_value_t *value;
} ten_value_kv_t;

TEN_UTILS_API bool ten_value_kv_check_integrity(ten_value_kv_t *self);

TEN_UTILS_API ten_value_kv_t *ten_value_kv_create_empty(const char *name);

TEN_UTILS_API ten_value_kv_t *ten_value_kv_create(const char *name,
                                                  ten_value_t *value);

TEN_UTILS_API void ten_value_kv_destroy(ten_value_kv_t *self);

// This special destroy function for the value will only deinitialize the key
// field and will not handle the value field.
TEN_UTILS_API void ten_value_kv_destroy_key_only(ten_value_kv_t *self);

TEN_UTILS_API ten_value_kv_t *ten_value_kv_clone(ten_value_kv_t *target);

TEN_UTILS_API ten_string_t *ten_value_kv_get_key(ten_value_kv_t *self);

TEN_UTILS_API ten_value_t *ten_value_kv_get_value(ten_value_kv_t *self);

TEN_UTILS_API void ten_value_kv_reset_to_value(ten_value_kv_t *self,
                                               ten_value_t *value);

TEN_UTILS_API ten_string_t *ten_value_kv_to_string(ten_value_kv_t *self,
                                                   ten_error_t *err);

TEN_UTILS_API ten_value_kv_t *ten_value_kv_from_json(const char *key,
                                                     ten_json_t *json);

TEN_UTILS_API bool ten_value_kv_to_json(ten_value_kv_t *self, ten_json_t *json);
