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

#include "ten_utils/lib/error.h"
#include "ten_utils/value/value_kv.h"

TEN_UTILS_API ten_value_t *ten_value_object_peek(ten_value_t *self,
                                                 const char *key);

TEN_UTILS_API bool ten_value_object_get_bool(ten_value_t *self, const char *key,
                                             ten_error_t *err);

TEN_UTILS_API const char *ten_value_object_peek_string(ten_value_t *self,
                                                       const char *key);

TEN_UTILS_API ten_list_t *ten_value_object_peek_array(ten_value_t *self,
                                                      const char *key);

/**
 * @note Note that the ownership of @a value is moved to the value @a self.
 */
TEN_UTILS_API bool ten_value_object_move(ten_value_t *self, const char *key,
                                         ten_value_t *value);
