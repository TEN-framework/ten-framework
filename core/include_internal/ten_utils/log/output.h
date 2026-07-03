//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/string.h"

/**
 * @brief String to put in the end of each log line (can be empty).
 */
#ifndef TEN_LOG_EOL
#define TEN_LOG_EOL "\n"
#endif

typedef struct ten_log_t ten_log_t;
typedef struct ten_string_t ten_string_t;
typedef struct ten_mutex_t ten_mutex_t;
typedef struct ten_log_output_t ten_log_output_t;

typedef struct ten_log_output_to_file_ctx_t {
  int *fd;
  ten_string_t log_path;
  ten_atomic_t need_reload;
  ten_mutex_t *mutex;
} ten_log_output_to_file_ctx_t;

TEN_UTILS_PRIVATE_API ten_log_output_to_file_ctx_t *
ten_log_output_to_file_ctx_create(int *fd, const char *log_path);

TEN_UTILS_PRIVATE_API void ten_log_output_to_file_ctx_destroy(
    ten_log_output_to_file_ctx_t *ctx);

TEN_UTILS_PRIVATE_API void ten_log_output_init(ten_log_output_t *self);

TEN_UTILS_API void ten_log_set_output_to_stderr(ten_log_t *self);

TEN_UTILS_PRIVATE_API void ten_log_output_to_file(ten_log_t *self,
                                                  ten_string_t *msg);

TEN_UTILS_PRIVATE_API void ten_log_output_to_stderr(ten_log_t *self,
                                                    ten_string_t *msg);

TEN_UTILS_PRIVATE_API void ten_log_set_output_to_file(ten_log_t *self,
                                                      const char *log_path);

TEN_UTILS_PRIVATE_API const char *ten_log_get_output_file_path(ten_log_t *self);
