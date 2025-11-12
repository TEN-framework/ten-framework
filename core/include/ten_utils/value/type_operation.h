//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/value/type.h"

TEN_UTILS_API TEN_TYPE ten_type_from_string(const char *type_str);

TEN_UTILS_API const char *ten_type_to_string(TEN_TYPE type);

TEN_UTILS_PRIVATE_API ten_list_t ten_type_from_json(ten_json_t *json);

TEN_UTILS_PRIVATE_API bool ten_type_is_compatible(TEN_TYPE actual,
                                                  TEN_TYPE expected);
