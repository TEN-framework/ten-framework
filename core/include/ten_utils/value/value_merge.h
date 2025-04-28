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

typedef struct ten_value_t ten_value_t;

TEN_UTILS_API bool ten_value_object_merge_with_move(ten_value_t *dest,
                                                    ten_value_t *src);

TEN_UTILS_API bool ten_value_object_merge_with_clone(ten_value_t *dest,
                                                     ten_value_t *src);

TEN_UTILS_API bool ten_value_object_merge_with_json(ten_value_t *dest,
                                                    ten_json_t *src);
