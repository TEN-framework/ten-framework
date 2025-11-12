//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/sanitizer/memory_check.h"  // IWYU pragma: keep

#if defined(TEN_ENABLE_MEMORY_CHECK)
#define TEN_STRNDUP(str, size) \
  ten_sanitizer_memory_strndup((str), (size), __FILE__, __LINE__, __FUNCTION__)
#else

#define TEN_STRNDUP(str, size) ten_strndup((str), (size))
#endif
