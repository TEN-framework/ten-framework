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
    {"json", ten_log_default_json_formatter},
    {"color_json", ten_log_colored_json_formatter},
};

static const size_t registered_formatters_size =
    sizeof(registered_formatters) / sizeof(ten_log_formatter_entry_t);

// Helper function to convert log level to string.
static const char *ten_log_level_to_string(TEN_LOG_LEVEL level) {
  switch (level) {
  case TEN_LOG_LEVEL_VERBOSE:
    return "VERBOSE";
  case TEN_LOG_LEVEL_DEBUG:
    return "DEBUG";
  case TEN_LOG_LEVEL_INFO:
    return "INFO";
  case TEN_LOG_LEVEL_WARN:
    return "WARN";
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

// Helper function to escape JSON string.
static void ten_json_escape_string(ten_string_t *dest, const char *src,
                                   size_t src_len) {
  TEN_ASSERT(dest, "Invalid argument.");
  TEN_ASSERT(src, "Invalid argument.");

  for (size_t i = 0; i < src_len; i++) {
    char c = src[i];
    switch (c) {
    case '"':
      ten_string_append_formatted(dest, "\\\"");
      break;
    case '\\':
      ten_string_append_formatted(dest, "\\\\");
      break;
    case '\n':
      ten_string_append_formatted(dest, "\\n");
      break;
    case '\r':
      ten_string_append_formatted(dest, "\\r");
      break;
    case '\t':
      ten_string_append_formatted(dest, "\\t");
      break;
    case '\b':
      ten_string_append_formatted(dest, "\\b");
      break;
    case '\f':
      ten_string_append_formatted(dest, "\\f");
      break;
    default:
      if (c >= 0 && c < 32) {
        ten_string_append_formatted(dest, "\\u%04x", (unsigned char)c);
      } else {
        ten_string_append_formatted(dest, "%c", c);
      }
      break;
    }
  }
}

// Helper function to format timestamp as ISO 8601 string.
static void ten_format_timestamp_iso8601(ten_string_t *dest,
                                         struct tm *time_info, size_t msec) {
  TEN_ASSERT(dest, "Invalid argument.");
  TEN_ASSERT(time_info, "Invalid argument.");

  ten_string_append_formatted(dest, "%04d-%02d-%02dT%02d:%02d:%02d.%03zuZ",
                              time_info->tm_year + 1900, time_info->tm_mon + 1,
                              time_info->tm_mday, time_info->tm_hour,
                              time_info->tm_min, time_info->tm_sec, msec);
}

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

// JSON formatter (no colors).
void ten_log_default_json_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
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

  ten_string_append_formatted(buf, "{");

  // Add timestamp.
  ten_string_append_formatted(buf, "\"timestamp\":\"");
  ten_format_timestamp_iso8601(buf, &time_info, msec);
  ten_string_append_formatted(buf, "\"");

  // Add level.
  ten_string_append_formatted(buf, ",\"level\":\"%s\"",
                              ten_log_level_to_string(level));

  // Add PID and TID.
  ten_string_append_formatted(buf, ",\"pid\":%" PRId64 ",\"tid\":%" PRId64, pid,
                              tid);

  // Add function name.
  if (func_name_len) {
    ten_string_append_formatted(buf, ",\"function\":\"");
    ten_json_escape_string(buf, func_name, func_name_len);
    ten_string_append_formatted(buf, "\"");
  }

  // Add file name and line number.
  size_t actual_file_name_len = 0;
  const char *actual_file_name =
      filename(file_name, file_name_len, &actual_file_name_len);
  if (actual_file_name_len) {
    ten_string_append_formatted(buf, ",\"file\":\"");
    ten_json_escape_string(buf, actual_file_name, actual_file_name_len);
    ten_string_append_formatted(buf, "\",\"line\":%zu", line_no);
  }

  // Add message.
  ten_string_append_formatted(buf, ",\"message\":\"");
  ten_json_escape_string(buf, msg, msg_len);
  ten_string_append_formatted(buf, "\"");

  ten_string_append_formatted(buf, "}");
}

// Colored JSON formatter.
void ten_log_colored_json_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
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

  ten_string_append_formatted(buf, "%s{%s", level_color, TEN_LOG_COLOR_RESET);

  // Add timestamp.
  ten_string_append_formatted(buf, "%s\"timestamp\":\"%s", TEN_LOG_COLOR_BLUE,
                              TEN_LOG_COLOR_RESET);
  ten_format_timestamp_iso8601(buf, &time_info, msec);
  ten_string_append_formatted(buf, "%s\"%s", TEN_LOG_COLOR_BLUE,
                              TEN_LOG_COLOR_RESET);

  // Add level.
  ten_string_append_formatted(
      buf, "%s,\"level\":\"%s%s%s\"%s", TEN_LOG_COLOR_BLUE, level_color,
      ten_log_level_to_string(level), TEN_LOG_COLOR_RESET, TEN_LOG_COLOR_BLUE);

  // Add PID and TID.
  ten_string_append_formatted(
      buf, ",\"pid\":%s%" PRId64 "%s,\"tid\":%s%" PRId64 "%s",
      TEN_LOG_COLOR_CYAN, pid, TEN_LOG_COLOR_RESET, TEN_LOG_COLOR_CYAN, tid,
      TEN_LOG_COLOR_RESET);

  // Add function name.
  if (func_name_len) {
    ten_string_append_formatted(buf, "%s,\"function\":\"%s", TEN_LOG_COLOR_BLUE,
                                TEN_LOG_COLOR_MAGENTA);
    ten_json_escape_string(buf, func_name, func_name_len);
    ten_string_append_formatted(buf, "%s\"%s", TEN_LOG_COLOR_RESET,
                                TEN_LOG_COLOR_BLUE);
  }

  // Add file name and line number.
  size_t actual_file_name_len = 0;
  const char *actual_file_name =
      filename(file_name, file_name_len, &actual_file_name_len);
  if (actual_file_name_len) {
    ten_string_append_formatted(buf, "%s,\"file\":\"%s", TEN_LOG_COLOR_BLUE,
                                TEN_LOG_COLOR_BLUE);
    ten_json_escape_string(buf, actual_file_name, actual_file_name_len);
    ten_string_append_formatted(buf, "%s\",\"line\":%s%zu%s",
                                TEN_LOG_COLOR_RESET, TEN_LOG_COLOR_CYAN,
                                line_no, TEN_LOG_COLOR_RESET);
  }

  // Add message.
  ten_string_append_formatted(buf, "%s,\"message\":\"%s", TEN_LOG_COLOR_BLUE,
                              TEN_LOG_COLOR_WHITE);
  ten_json_escape_string(buf, msg, msg_len);
  ten_string_append_formatted(buf, "%s\"%s", TEN_LOG_COLOR_RESET,
                              TEN_LOG_COLOR_BLUE);

  ten_string_append_formatted(buf, "%s}%s", TEN_LOG_COLOR_RESET, level_color);
}
