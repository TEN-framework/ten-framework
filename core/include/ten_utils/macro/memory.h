//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/alloc.h"
#include "ten_utils/sanitizer/memory_check.h"  // IWYU pragma: keep

#if defined(TEN_ENABLE_MEMORY_CHECK)

#define TEN_MALLOC(size) \
  ten_sanitizer_memory_malloc((size), __FILE__, __LINE__, __FUNCTION__)

#define TEN_CALLOC(cnt, size) \
  ten_sanitizer_memory_calloc((cnt), (size), __FILE__, __LINE__, __FUNCTION__)

#define TEN_FREE(address)                         \
  do {                                            \
    ten_sanitizer_memory_free((void *)(address)); \
    address = NULL;                               \
  } while (0)

#define TEN_FREE_(address)                        \
  do {                                            \
    ten_sanitizer_memory_free((void *)(address)); \
  } while (0)

#define TEN_REALLOC(address, size)                                    \
  ten_sanitizer_memory_realloc((address), (size), __FILE__, __LINE__, \
                               __FUNCTION__)

#define TEN_STRDUP(str) \
  ten_sanitizer_memory_strdup((str), __FILE__, __LINE__, __FUNCTION__)

#else

#define TEN_MALLOC(size) ten_malloc((size))

#define TEN_CALLOC(cnt, size) ten_calloc((cnt), (size))

#define TEN_FREE(address)        \
  do {                           \
    ten_free((void *)(address)); \
    address = NULL;              \
  } while (0)

#define TEN_FREE_(address)       \
  do {                           \
    ten_free((void *)(address)); \
  } while (0)

#define TEN_REALLOC(address, size) ten_realloc((address), (size))

#define TEN_STRDUP(str) ten_strdup((str))

#endif
