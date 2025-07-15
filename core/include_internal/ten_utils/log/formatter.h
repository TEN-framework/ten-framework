//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <time.h>

#include "ten_utils/log/log.h"

typedef void (*ten_log_formatter_on_format_func_t)(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_UTILS_PRIVATE_API void ten_json_escape_string(ten_string_t *dest,
                                                  const char *src,
                                                  size_t src_len);

TEN_UTILS_PRIVATE_API void ten_format_timestamp_iso8601(ten_string_t *dest,
                                                        struct tm *time_info,
                                                        size_t msec);

TEN_UTILS_PRIVATE_API const char *ten_log_level_to_string(TEN_LOG_LEVEL level);

TEN_UTILS_PRIVATE_API void ten_log_plain_formatter(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_UTILS_PRIVATE_API void ten_log_plain_colored_formatter(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_UTILS_PRIVATE_API void ten_log_json_formatter(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_UTILS_PRIVATE_API void ten_log_json_colored_formatter(
    ten_string_t *buf, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *msg, size_t msg_len);

TEN_UTILS_PRIVATE_API void ten_log_set_formatter(
    ten_log_t *self, ten_log_formatter_on_format_func_t format_cb,
    void *user_data);

TEN_UTILS_PRIVATE_API ten_log_formatter_on_format_func_t
ten_log_get_formatter_by_name(const char *name);
