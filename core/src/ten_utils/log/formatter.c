//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/log/formatter.h"

#include <inttypes.h>
#include <time.h>

#include "include_internal/ten_utils/lib/time.h"
#include "include_internal/ten_utils/log/level.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/log/termcolor.h"
#include "ten_utils/lib/pid.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

typedef struct ten_log_formatter_entry_t {
  const char *name;
  ten_log_formatter_on_format_func_t formatter_func;
} ten_log_formatter_entry_t;

static ten_log_formatter_entry_t registered_formatters[] = {
    {"default", ten_log_default_formatter},
    {"color", ten_log_colored_formatter},
};

static const size_t registered_formatters_size =
    sizeof(registered_formatters) / sizeof(ten_log_formatter_entry_t);

ten_log_formatter_on_format_func_t ten_log_get_formatter_by_name(
    const char *name) {
  TEN_ASSERT(name, "Invalid argument.");

  ten_log_formatter_on_format_func_t result = NULL;

  for (size_t i = 0; i < registered_formatters_size; i++) {
    if (strcmp(registered_formatters[i].name, name) == 0) {
      return registered_formatters[i].formatter_func;
    }
  }

  return NULL;
}

void ten_log_set_formatter(ten_log_t *self,
                           ten_log_formatter_on_format_func_t format_cb,
                           void *user_data) {
  TEN_ASSERT(self, "Invalid argument.");

  self->formatter.on_format = format_cb;
  self->formatter.user_data = user_data;
}

// Default formatter (no colors).
void ten_log_default_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
                               const char *func_name, size_t func_name_len,
                               const char *file_name, size_t file_name_len,
                               size_t line_no, const char *msg,
                               size_t msg_len) {
  struct tm time_info;
  size_t msec = 0;

  ten_current_time_info(&time_info, &msec);
  ten_string_append_time_info(buf, &time_info, msec);

  int64_t pid = 0;
  int64_t tid = 0;
  ten_get_pid_tid(&pid, &tid);

  ten_string_append_formatted(buf, " %d(%d) %c", pid, tid,
                              ten_log_level_char(level));

  if (func_name_len) {
    ten_string_append_formatted(buf, " %.*s", (int)func_name_len, func_name);
  }

  size_t actual_file_name_len = 0;
  const char *actual_file_name =
      filename(file_name, file_name_len, &actual_file_name_len);
  if (actual_file_name_len) {
    ten_string_append_formatted(buf, "@%.*s:%d", (int)actual_file_name_len,
                                actual_file_name, (int)line_no);
  }

  ten_string_append_formatted(buf, " %.*s", (int)msg_len, msg);
}

// Colored formatter.
void ten_log_colored_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
                               const char *func_name, size_t func_name_len,
                               const char *file_name, size_t file_name_len,
                               size_t line_no, const char *msg,
                               size_t msg_len) {
  struct tm time_info;
  size_t msec = 0;
  ten_current_time_info(&time_info, &msec);
  ten_string_append_time_info(buf, &time_info, msec);

  int64_t pid = 0;
  int64_t tid = 0;
  ten_get_pid_tid(&pid, &tid);

  // Determine color based on log level.
  const char *level_color = NULL;
  switch (level) {
  case TEN_LOG_LEVEL_FATAL:
  case TEN_LOG_LEVEL_ERROR:
    level_color = TEN_LOG_COLOR_RED;
    break;
  case TEN_LOG_LEVEL_WARN:
    level_color = TEN_LOG_COLOR_YELLOW;
    break;
  case TEN_LOG_LEVEL_INFO:
    level_color = TEN_LOG_COLOR_GREEN;
    break;
  case TEN_LOG_LEVEL_DEBUG:
  case TEN_LOG_LEVEL_VERBOSE:
    level_color = TEN_LOG_COLOR_CYAN;
    break;
  default:
    level_color = TEN_LOG_COLOR_WHITE;
    break;
  }

  ten_string_append_formatted(buf, " %" PRId64 "(%" PRId64 ") %s%c%s", pid, tid,
                              level_color, ten_log_level_char(level),
                              TEN_LOG_COLOR_RESET);

  // Add color to function name.
  if (func_name_len) {
    ten_string_append_formatted(buf, " %s%.*s%s", TEN_LOG_COLOR_MAGENTA,
                                (int)func_name_len, func_name,
                                TEN_LOG_COLOR_RESET);
  }

  // Add color to file name and line number.
  size_t actual_file_name_len = 0;
  const char *actual_file_name =
      filename(file_name, file_name_len, &actual_file_name_len);
  if (actual_file_name_len) {
    ten_string_append_formatted(buf, "%s@%.*s:%zu%s", TEN_LOG_COLOR_BLUE,
                                (int)actual_file_name_len, actual_file_name,
                                line_no, TEN_LOG_COLOR_RESET);
  }

  // Add color to message.
  ten_string_append_formatted(buf, " %s%.*s%s", TEN_LOG_COLOR_WHITE,
                              (int)msg_len, msg, TEN_LOG_COLOR_RESET);
}
