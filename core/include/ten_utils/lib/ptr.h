//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "ten_utils/macro/check.h"

#define PTR_FILL_VALUE(ptr, value)                  \
  ({                                                \
    *(typeof(value) *)ptr = value;                  \
    ptr = ten_ptr_move_in_byte(ptr, sizeof(value)); \
  })

#define PTR_FILL_STR(ptr, str)                    \
  ({                                              \
    *(typeof(value) *)ptr = value;                \
    strcpy(ptr, str);                             \
    ptr = ten_ptr_move_in_byte(ptr, strlen(str)); \
  })

// NOLINTNEXTLINE(clang-diagnostic-unused-function)
static inline void *ten_ptr_move_in_byte(void *ptr, ptrdiff_t offset) {
  TEN_ASSERT(ptr, "Invalid pointer.");
  return (void *)((uint8_t *)ptr + offset);
}

// NOLINTNEXTLINE(clang-diagnostic-unused-function)
static inline const void *ten_const_ptr_move_in_byte(const void *ptr,
                                                     ptrdiff_t offset) {
  TEN_ASSERT(ptr, "Invalid pointer.");
  return (const void *)((const uint8_t *)ptr + offset);
}

// NOLINTNEXTLINE(clang-diagnostic-unused-function)
static inline ptrdiff_t ten_ptr_diff_in_byte(void *a, void *b) {
  TEN_ASSERT(a && b && a >= b, "Invalid pointers.");
  return (uint8_t *)a - (uint8_t *)b;
}

// NOLINTNEXTLINE(clang-diagnostic-unused-function)
static inline ptrdiff_t ten_const_ptr_diff_in_byte(const void *a,
                                                   const void *b) {
  TEN_ASSERT(a && b && a >= b, "Invalid pointers.");
  return (const uint8_t *)a - (const uint8_t *)b;
}
