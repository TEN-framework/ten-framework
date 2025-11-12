//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/backtrace.h"

typedef struct ten_backtrace_common_t {
  ten_backtrace_on_dump_file_line_func_t on_dump_file_line;
  ten_backtrace_on_error_func_t on_error;
  void *cb_data;  // The user-defined argument to the above callback functions.
} ten_backtrace_common_t;

TEN_UTILS_PRIVATE_API ten_backtrace_t *g_ten_backtrace;

TEN_UTILS_PRIVATE_API void ten_backtrace_common_init(
    ten_backtrace_common_t *self,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error);

TEN_UTILS_PRIVATE_API void ten_backtrace_common_deinit(ten_backtrace_t *self);

TEN_UTILS_PRIVATE_API int ten_backtrace_default_dump(
    ten_backtrace_t *self, uintptr_t pc, const char *filename, int lineno,
    const char *function, void *data);

TEN_UTILS_PRIVATE_API void ten_backtrace_default_error(ten_backtrace_t *self,
                                                       const char *msg,
                                                       int errnum, void *data);
