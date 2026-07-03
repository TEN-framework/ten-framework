//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <time.h>

#if !defined(_WIN32) && !defined(_WIN64)

#include <sys/time.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/limits.h>
#elif defined(__MACH__)
#include <sys/syslimits.h>
#endif

TEN_UTILS_PRIVATE_API bool ten_time_cache_get(const struct timeval *tv,
                                              struct tm *tm);

TEN_UTILS_PRIVATE_API void ten_time_cache_set(const struct timeval *tv,
                                              struct tm *tm);

#endif
