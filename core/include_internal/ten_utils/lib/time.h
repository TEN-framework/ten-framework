//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <time.h>

#include "ten_utils/lib/string.h"

// Maximum length of "MM-DD HH:MM:SS.XXX"
//                     212 1 212 12 1 3 = 18
#define TIME_INFO_STRING_LEN (size_t)(18 * 2)

TEN_UTILS_PRIVATE_API void ten_current_time_info(struct tm *time_info,
                                                 size_t *msec);

TEN_UTILS_PRIVATE_API void ten_string_append_time_info(ten_string_t *str,
                                                       struct tm *time_info,
                                                       size_t msec);
