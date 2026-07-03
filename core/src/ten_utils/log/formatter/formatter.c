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

#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

typedef struct ten_log_formatter_entry_t {
  const char *name;
  ten_log_formatter_on_format_func_t formatter_func;
} ten_log_formatter_entry_t;

static ten_log_formatter_entry_t registered_formatters[] = {
    {"plain", ten_log_plain_formatter},
    {"plain_colored", ten_log_plain_colored_formatter},
    {"json", ten_log_json_formatter},
    {"json_colored", ten_log_json_colored_formatter},
};

static const size_t registered_formatters_size =
    sizeof(registered_formatters) / sizeof(ten_log_formatter_entry_t);

// Helper function to convert log level to string.
const char *ten_log_level_to_string(TEN_LOG_LEVEL level) {
  switch (level) {
  case TEN_LOG_LEVEL_DEBUG:
    return "DEBUG";
  case TEN_LOG_LEVEL_INFO:
    return "INFO";
  case TEN_LOG_LEVEL_WARN:
    return "WARN";
  case TEN_LOG_LEVEL_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

// Helper function to escape JSON string.
void ten_json_escape_string(ten_string_t *dest, const char *src,
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
void ten_format_timestamp_iso8601(ten_string_t *dest, struct tm *time_info,
                                  size_t msec) {
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
