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

#include "ten_utils/log/log.h"

#define TEN_LOG_SIGNATURE 0xC0EE0CE92149D61AU

#define TEN_LOGM(...)                                                         \
  do {                                                                        \
    ten_log_log_formatted(&ten_global_log, TEN_LOG_LEVEL_MANDATORY, __func__, \
                          __FILE__, __LINE__, __VA_ARGS__);                   \
  } while (0)

#define TEN_LOGV_AUX(log, ...)                                            \
  do {                                                                    \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_VERBOSE, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                         \
  } while (0)

#define TEN_LOGD_AUX(log, ...)                                          \
  do {                                                                  \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_DEBUG, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                       \
  } while (0)

#define TEN_LOGI_AUX(log, ...)                                         \
  do {                                                                 \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_INFO, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                      \
  } while (0)

#define TEN_LOGW_AUX(log, ...)                                         \
  do {                                                                 \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_WARN, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                      \
  } while (0)

#define TEN_LOGE_AUX(log, ...)                                          \
  do {                                                                  \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_ERROR, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                       \
  } while (0)

#define TEN_LOGF_AUX(log, ...)                                          \
  do {                                                                  \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_FATAL, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                       \
  } while (0)

#define TEN_LOGM_AUX(log, ...)                                              \
  do {                                                                      \
    ten_log_log_formatted(log, TEN_LOG_LEVEL_MANDATORY, __func__, __FILE__, \
                          __LINE__, __VA_ARGS__);                           \
  } while (0)

typedef struct ten_string_t ten_string_t;

TEN_UTILS_PRIVATE_API bool ten_log_check_integrity(ten_log_t *self);

TEN_UTILS_API void ten_log_init(ten_log_t *self);

TEN_UTILS_PRIVATE_API ten_log_t *ten_log_create(void);

TEN_UTILS_API void ten_log_deinit(ten_log_t *self);

TEN_UTILS_PRIVATE_API void ten_log_deinit_encryption(ten_log_t *self);

TEN_UTILS_PRIVATE_API void ten_log_destroy(ten_log_t *self);

TEN_UTILS_PRIVATE_API void ten_log_set_encrypt_cb(
    ten_log_t *self, ten_log_encrypt_on_encrypt_func_t cb, void *cb_data);

TEN_UTILS_PRIVATE_API void ten_log_reload(ten_log_t *self);

TEN_UTILS_PRIVATE_API void ten_log_set_encrypt_deinit_cb(
    ten_log_t *self, ten_log_encrypt_on_deinit_func_t cb);

TEN_UTILS_PRIVATE_API const char *filename(const char *path, size_t path_len,
                                           size_t *filename_len);

TEN_UTILS_API void ten_log_log(ten_log_t *self, TEN_LOG_LEVEL level,
                               const char *func_name, const char *file_name,
                               size_t line_no, const char *msg);

TEN_UTILS_API void ten_log_log_with_size(ten_log_t *self, TEN_LOG_LEVEL level,
                                         const char *func_name,
                                         size_t func_name_len,
                                         const char *file_name,
                                         size_t file_name_len, size_t line_no,
                                         const char *msg, size_t msg_len);

TEN_UTILS_API void ten_log_global_init(void);

TEN_UTILS_API void ten_log_global_deinit(void);

TEN_UTILS_API void ten_log_global_set_output_level(TEN_LOG_LEVEL level);

TEN_UTILS_API void ten_log_global_set_output_to_stderr(void);

TEN_UTILS_API void ten_log_global_set_output_to_file(const char *log_path);

TEN_UTILS_API const char *ten_log_global_get_output_file_path(void);

TEN_UTILS_API void ten_log_global_set_encrypt_cb(
    ten_log_encrypt_on_encrypt_func_t cb, void *cb_data);

TEN_UTILS_API void ten_log_global_set_encrypt_deinit_cb(
    ten_log_encrypt_on_deinit_func_t cb);

TEN_UTILS_API void ten_log_global_deinit_encryption(void);

TEN_UTILS_API void ten_log_global_deinit_advanced_log(void);

TEN_UTILS_API void ten_log_global_reload(void);

TEN_UTILS_API void ten_log_global_set_advanced_impl_with_config(
    ten_log_advanced_log_func_t impl,
    ten_log_advanced_log_config_on_deinit_func_t on_deinit, void *config);

TEN_UTILS_API void ten_log_global_set_advanced_log_reloadable(void);

TEN_UTILS_API bool ten_log_global_is_advanced_log_reloadable(void);

TEN_UTILS_API void ten_log_advanced_impl_init(ten_log_advanced_impl_t *self);

TEN_UTILS_API void ten_log_advanced_impl_deinit(ten_log_advanced_impl_t *self);

TEN_UTILS_API void ten_log_set_advanced_impl_with_config(
    ten_log_t *self, ten_log_advanced_log_func_t impl,
    ten_log_advanced_log_config_on_deinit_func_t on_deinit, void *config);
