//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_env/internal/log.h"

#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/string.h"

#if !defined(OS_WINDOWS)
#include <unistd.h>
#endif

static void ten_env_log_internal(ten_env_t *self, TEN_LOG_LEVEL level,
                                 const char *func_name, const char *file_name,
                                 size_t line_no, const char *msg,
                                 const char *category, ten_value_t *fields,
                                 bool check_thread) {
  TEN_ASSERT(self && ten_env_check_integrity(self, check_thread),
             "Should not happen.");

  ten_string_t final_msg;
  ten_string_init_formatted(
      &final_msg, "[%s] %s",
      ten_env_get_attached_instance_name(self, check_thread), msg);

  ten_log_log(&ten_global_log, level, func_name, file_name, line_no,
              ten_string_get_raw_str(&final_msg), category, fields);

  ten_string_deinit(&final_msg);
}

// TODO(Wei): This function is currently specifically designed for the addon
// because the addon currently does not have a main thread, so it's unable to
// check thread safety. Once the main thread for the addon is determined in the
// future, these hacks made specifically for the addon can be completely
// removed, and comprehensive thread safety checking can be implemented.
TEN_RUNTIME_API void ten_env_log_without_check_thread(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    const char *file_name, size_t line_no, const char *msg,
    const char *category, ten_value_t *fields) {
  ten_env_log_internal(self, level, func_name, file_name, line_no, msg,
                       category, fields, false);
}

void ten_env_log(ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
                 const char *file_name, size_t line_no, const char *msg,
                 const char *category, ten_value_t *fields) {
  if (ten_env_is_closed(self)) {
#if !defined(OS_WINDOWS)
    (void)dprintf(STDERR_FILENO, "ten_env_log failed due to closed: %s\n", msg);
#endif
    return;
  }

  ten_env_log_internal(self, level, func_name, file_name, line_no, msg,
                       category, fields, true);
}

static void ten_env_log_with_size_formatted_internal(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, bool check_thread, const char *category,
    size_t category_len, ten_value_t *fields, const char *fmt, va_list ap) {
  TEN_ASSERT(self && ten_env_check_integrity(self, check_thread),
             "Should not happen.");

  ten_string_t final_msg;
  ten_string_init_formatted(
      &final_msg, "[%s] ",
      ten_env_get_attached_instance_name(self, check_thread));

  ten_string_append_from_va_list(&final_msg, fmt, ap);

  ten_log_log_with_size(
      &ten_global_log, level, func_name, func_name_len, file_name,
      file_name_len, line_no, ten_string_get_raw_str(&final_msg),
      ten_string_len(&final_msg), category, category_len, fields);

  ten_string_deinit(&final_msg);
}

// TODO(Wei): This function is currently specifically designed for the addon
// because the addon currently does not have a main thread, so it's unable to
// check thread safety. Once the main thread for the addon is determined in the
// future, these hacks made specifically for the addon can be completely
// removed, and comprehensive thread safety checking can be implemented.
void ten_env_log_with_size_formatted_without_check_thread(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *category, size_t category_len,
    ten_value_t *fields, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  ten_env_log_with_size_formatted_internal(
      self, level, func_name, func_name_len, file_name, file_name_len, line_no,
      false, category, category_len, fields, fmt, ap);

  va_end(ap);
}

void ten_env_log_with_size_formatted(
    ten_env_t *self, TEN_LOG_LEVEL level, const char *func_name,
    size_t func_name_len, const char *file_name, size_t file_name_len,
    size_t line_no, const char *category, size_t category_len,
    ten_value_t *fields, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  ten_env_log_with_size_formatted_internal(
      self, level, func_name, func_name_len, file_name, file_name_len, line_no,
      true, category, category_len, fields, fmt, ap);

  va_end(ap);
}

void ten_env_log_formatted(ten_env_t *self, TEN_LOG_LEVEL level,
                           const char *func_name, const char *file_name,
                           size_t line_no, const char *category,
                           size_t category_len, ten_value_t *fields,
                           const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  ten_env_log_with_size_formatted_internal(
      self, level, func_name, strlen(func_name), file_name, strlen(file_name),
      line_no, true, category, category_len, fields, fmt, ap);

  va_end(ap);
}
