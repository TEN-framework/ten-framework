//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/log/log.h"

#define TEN_ENV_LOG_VERBOSE_INTERNAL(ten_env, ...)                            \
  do {                                                                        \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_VERBOSE, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                             \
  } while (0)

#define TEN_ENV_LOG_DEBUG_INTERNAL(ten_env, ...)                            \
  do {                                                                      \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_DEBUG, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                           \
  } while (0)

#define TEN_ENV_LOG_INFO_INTERNAL(ten_env, ...)                            \
  do {                                                                     \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_INFO, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                          \
  } while (0)

#define TEN_ENV_LOG_WARN_INTERNAL(ten_env, ...)                            \
  do {                                                                     \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_WARN, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                          \
  } while (0)

#define TEN_ENV_LOG_ERROR_INTERNAL(ten_env, ...)                            \
  do {                                                                      \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_ERROR, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                           \
  } while (0)

#define TEN_ENV_LOG_FATAL_INTERNAL(ten_env, ...)                            \
  do {                                                                      \
    ten_env_log_formatted(ten_env, TEN_LOG_LEVEL_FATAL, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                           \
  } while (0)

typedef struct ten_env_t ten_env_t;

TEN_RUNTIME_API void ten_env_log_with_size_formatted_without_check_thread(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *fmt, ...);

TEN_RUNTIME_API void ten_env_log_with_size_formatted(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *fmt, ...);

TEN_RUNTIME_PRIVATE_API void ten_env_log_formatted(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    const char *file_name, size_t line_no, const char *fmt, ...);

TEN_RUNTIME_API void ten_env_log_without_check_thread(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    const char *file_name, size_t line_no, const char *msg);
