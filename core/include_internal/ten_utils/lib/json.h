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

TEN_UTILS_API bool ten_json_object_set_string(ten_json_t *self, const char *key,
                                              const char *value);

TEN_UTILS_API bool ten_json_object_set_int(ten_json_t *self, const char *key,
                                           int64_t value);

TEN_UTILS_API bool ten_json_object_set_real(ten_json_t *self, const char *key,
                                            double value);

TEN_UTILS_API bool ten_json_object_set_bool(ten_json_t *self, const char *key,
                                            bool value);
