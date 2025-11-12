//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

typedef struct ten_backtrace_t ten_backtrace_t;

typedef struct ten_backtrace_buffer_t {
  char *data;
  size_t capacity;
  size_t length;
  int overflow;
} ten_backtrace_buffer_t;

TEN_UTILS_PRIVATE_API void ten_backtrace_buffer_init(
    ten_backtrace_buffer_t *self, char *data, size_t capacity);

TEN_UTILS_PRIVATE_API int ten_backtrace_buffer_dump(
    ten_backtrace_t *self, uintptr_t pc, const char *filename, int lineno,
    const char *function, void *data);
