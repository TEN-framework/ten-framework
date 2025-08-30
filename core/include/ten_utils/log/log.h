//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/lib/signature.h"

#define TEN_LOG_CATEGORY_DEFAULT "ten:runtime"

#define TEN_LOGD(...)                                                         \
  do {                                                                        \
    ten_log_log_formatted(&ten_global_log, TEN_LOG_LEVEL_DEBUG, __func__,     \
                          __FILE__, __LINE__, TEN_LOG_CATEGORY_DEFAULT, NULL, \
                          __VA_ARGS__);                                       \
  } while (0)

#define TEN_LOGI(...)                                                         \
  do {                                                                        \
    ten_log_log_formatted(&ten_global_log, TEN_LOG_LEVEL_INFO, __func__,      \
                          __FILE__, __LINE__, TEN_LOG_CATEGORY_DEFAULT, NULL, \
                          __VA_ARGS__);                                       \
  } while (0)

#define TEN_LOGW(...)                                                         \
  do {                                                                        \
    ten_log_log_formatted(&ten_global_log, TEN_LOG_LEVEL_WARN, __func__,      \
                          __FILE__, __LINE__, TEN_LOG_CATEGORY_DEFAULT, NULL, \
                          __VA_ARGS__);                                       \
  } while (0)

#define TEN_LOGE(...)                                                         \
  do {                                                                        \
    ten_log_log_formatted(&ten_global_log, TEN_LOG_LEVEL_ERROR, __func__,     \
                          __FILE__, __LINE__, TEN_LOG_CATEGORY_DEFAULT, NULL, \
                          __VA_ARGS__);                                       \
  } while (0)

typedef struct ten_string_t ten_string_t;
typedef struct ten_log_t ten_log_t;
typedef struct ten_value_t ten_value_t;

typedef enum TEN_LOG_LEVEL {
  TEN_LOG_LEVEL_INVALID,

  TEN_LOG_LEVEL_DEBUG,
  TEN_LOG_LEVEL_INFO,
  TEN_LOG_LEVEL_WARN,
  TEN_LOG_LEVEL_ERROR,
} TEN_LOG_LEVEL;

typedef enum TEN_LOG_OUTPUT_TYPE {
  TEN_LOG_OUTPUT_TYPE_INVALID,

  TEN_LOG_OUTPUT_TYPE_FILE,
  TEN_LOG_OUTPUT_TYPE_STDERR,
} TEN_LOG_OUTPUT_TYPE;

typedef void (*ten_log_output_on_output_func_t)(ten_log_t *self,
                                                ten_string_t *msg);
typedef void (*ten_log_output_on_close_func_t)(ten_log_t *self);
typedef void (*ten_log_output_on_reload_func_t)(ten_log_t *self);
typedef void (*ten_log_output_on_deinit_func_t)(ten_log_t *self);

typedef void (*ten_log_formatter_on_format_func_t)(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

typedef void (*ten_log_encrypt_on_encrypt_func_t)(uint8_t *data,
                                                  size_t data_len,
                                                  void *user_data);
typedef void (*ten_log_encrypt_on_deinit_func_t)(void *user_data);

typedef void (*ten_log_advanced_log_func_t)(
    ten_log_t *self, TEN_LOG_LEVEL level, const char *category,
    size_t category_len, const char *func_name, size_t func_name_len,
    const char *file_name, size_t file_name_len, size_t line_no,
    const char *msg, size_t msg_len, ten_value_t *fields);

typedef void (*ten_log_advanced_log_reopen_all_func_t)(ten_log_t *self,
                                                       void *config);

typedef void (*ten_log_advanced_log_config_on_deinit_func_t)(void *config);

typedef struct ten_log_output_t {
  ten_log_output_on_output_func_t on_output;
  ten_log_output_on_close_func_t on_close;
  ten_log_output_on_reload_func_t on_reload;
  ten_log_output_on_deinit_func_t on_deinit;

  TEN_LOG_OUTPUT_TYPE type;
  void *user_data;
} ten_log_output_t;

typedef struct ten_log_encryption_t {
  ten_log_encrypt_on_encrypt_func_t on_encrypt;
  ten_log_encrypt_on_deinit_func_t on_deinit;
  void *impl;
} ten_log_encryption_t;

typedef struct ten_log_formatter_t {
  ten_log_formatter_on_format_func_t on_format;
  void *user_data;  // In case the formatter needs any user data
} ten_log_formatter_t;

typedef struct ten_log_advanced_impl_t {
  ten_log_advanced_log_func_t impl;
  ten_log_advanced_log_config_on_deinit_func_t on_deinit;
  ten_log_advanced_log_reopen_all_func_t reopen_all;

  // The reloadable feature is for internal use only and is used in smoke tests
  bool is_reloadable;

  void *config;
} ten_log_advanced_impl_t;

typedef struct ten_log_t {
  ten_signature_t signature;

  // Deprecated fields.
  TEN_LOG_LEVEL output_level;
  ten_log_output_t output;
  ten_log_formatter_t formatter;
  ten_log_encryption_t encryption;

  ten_log_advanced_impl_t advanced_impl;
  bool enable_advanced_log;
} ten_log_t;

TEN_UTILS_API ten_log_t ten_global_log;

TEN_UTILS_API void ten_log_log_formatted(ten_log_t *self, TEN_LOG_LEVEL level,
                                         const char *func_name,
                                         const char *file_name, size_t line_no,
                                         const char *category,
                                         ten_value_t *fields, const char *fmt,
                                         ...);
