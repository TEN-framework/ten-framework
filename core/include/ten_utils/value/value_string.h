//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/string.h"
#include "ten_utils/value/value_kv.h"

TEN_UTILS_API bool ten_value_to_string(ten_value_t *self, ten_string_t *str,
                                       ten_error_t *err);

TEN_UTILS_API ten_value_t *ten_value_from_type_and_string(TEN_TYPE type,
                                                          const char *str,
                                                          ten_error_t *err);
