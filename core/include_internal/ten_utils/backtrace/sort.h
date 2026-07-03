//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

/**
 * @brief Sort without using memory.
 */
TEN_UTILS_API void backtrace_sort(void *base, size_t count, size_t size,
                                  int (*compar)(const void *, const void *));
