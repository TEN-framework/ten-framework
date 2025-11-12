//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

TEN_UTILS_API void ten_backtrace_dump_global(size_t skip);

#if defined(__cplusplus)
} /* End extern "C".  */
#endif
