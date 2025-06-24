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
#include <stdlib.h>

#include "include_internal/ten_utils/lib/safe_cast.h"
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
    {"json", ten_log_json_formatter},
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

  ten_string_append_formatted(buf, " %" PRId64 "(%" PRId64 ") %c", pid, tid,
                              ten_log_level_char(level));

  if (func_name_len) {
    int int_func_name_len = 0;
    bool rc = safe_cast_size_t_to_int(func_name_len, &int_func_name_len);
    TEN_ASSERT(rc, "Function name length overflow detected.");

    ten_string_append_formatted(buf, " %.*s", int_func_name_len, func_name);
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
  case TEN_LOG_LEVEL_MANDATORY:
    level_color = TEN_LOG_COLOR_GOLD;
    break;
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

// Helper function to get log level string for JSON format
static const char *ten_log_level_string(TEN_LOG_LEVEL level) {
  switch (level) {
  case TEN_LOG_LEVEL_VERBOSE:
    return "VERBOSE";
  case TEN_LOG_LEVEL_DEBUG:
    return "DEBUG";
  case TEN_LOG_LEVEL_INFO:
    return "INFO";
  case TEN_LOG_LEVEL_WARN:
    return "WARNING";
  case TEN_LOG_LEVEL_ERROR:
    return "ERROR";
  case TEN_LOG_LEVEL_FATAL:
    return "FATAL";
  case TEN_LOG_LEVEL_MANDATORY:
    return "MANDATORY";
  default:
    return "UNKNOWN";
  }
}

// Helper function to escape JSON strings
static void ten_string_append_json_escaped(ten_string_t *buf, const char *str, size_t str_len) {
  if (!str || str_len == 0) {
    return;
  }

  for (size_t i = 0; i < str_len; i++) {
    char c = str[i];
    switch (c) {
    case '"':
      ten_string_append_formatted(buf, "\\\"");
      break;
    case '\\':
      ten_string_append_formatted(buf, "\\\\");
      break;
    case '\b':
      ten_string_append_formatted(buf, "\\b");
      break;
    case '\f':
      ten_string_append_formatted(buf, "\\f");
      break;
    case '\n':
      ten_string_append_formatted(buf, "\\n");
      break;
    case '\r':
      ten_string_append_formatted(buf, "\\r");
      break;
    case '\t':
      ten_string_append_formatted(buf, "\\t");
      break;
    default:
      if (c < 0x20) {
        ten_string_append_formatted(buf, "\\u%04x", (unsigned char)c);
      } else {
        ten_string_append_formatted(buf, "%c", c);
      }
      break;
    }
  }
}

// Helper function to format timestamp in ISO 8601 format for JSON
static void ten_string_append_json_timestamp(ten_string_t *buf, struct tm *time_info, size_t msec) {
  ten_string_append_formatted(buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03zuZ",
                              time_info->tm_year + 1900,
                              time_info->tm_mon + 1,
                              time_info->tm_mday,
                              time_info->tm_hour,
                              time_info->tm_min,
                              time_info->tm_sec,
                              msec);
}

// JSON formatter (GCP Cloud Logging compatible).
void ten_log_json_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
                            const char *func_name, size_t func_name_len,
                            const char *file_name, size_t file_name_len,
                            size_t line_no, const char *msg,
                            size_t msg_len) {
  struct tm time_info;
  size_t msec = 0;

  ten_current_time_info(&time_info, &msec);

  int64_t pid = 0;
  int64_t tid = 0;
  ten_get_pid_tid(&pid, &tid);

  // Start JSON object
  ten_string_append_formatted(buf, "{");

  // Add timestamp in ISO 8601 format
  ten_string_append_formatted(buf, "\"timestamp\":\"");
  ten_string_append_json_timestamp(buf, &time_info, msec);
  ten_string_append_formatted(buf, "\"");

  // Add severity (GCP compatible)
  ten_string_append_formatted(buf, ",\"severity\":\"%s\"", ten_log_level_string(level));

  // Add message
  ten_string_append_formatted(buf, ",\"message\":\"");
  ten_string_append_json_escaped(buf, msg, msg_len);
  ten_string_append_formatted(buf, "\"");

  // Add source location if available
  if (func_name_len > 0 || file_name_len > 0) {
    ten_string_append_formatted(buf, ",\"sourceLocation\":{");

    if (file_name_len > 0) {
      size_t actual_file_name_len = 0;
      const char *actual_file_name = filename(file_name, file_name_len, &actual_file_name_len);

      ten_string_append_formatted(buf, "\"file\":\"");
      ten_string_append_json_escaped(buf, actual_file_name, actual_file_name_len);
      ten_string_append_formatted(buf, "\"");

      if (line_no > 0) {
        ten_string_append_formatted(buf, ",\"line\":%zu", line_no);
      }
    }

    if (func_name_len > 0) {
      if (file_name_len > 0) {
        ten_string_append_formatted(buf, ",");
      }
      ten_string_append_formatted(buf, "\"function\":\"");
      ten_string_append_json_escaped(buf, func_name, func_name_len);
      ten_string_append_formatted(buf, "\"");
    }

    ten_string_append_formatted(buf, "}");
  }

  // Add process and thread IDs
  ten_string_append_formatted(buf, ",\"pid\":%" PRId64 ",\"tid\":%" PRId64, pid, tid);

  // Close JSON object
  ten_string_append_formatted(buf, "}");
}