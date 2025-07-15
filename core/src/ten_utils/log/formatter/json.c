//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include <inttypes.h>
#include <time.h>

#include "include_internal/ten_utils/lib/time.h"
#include "include_internal/ten_utils/log/formatter.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/pid.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

void ten_log_json_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
                            const char *func_name, size_t func_name_len,
                            const char *file_name, size_t file_name_len,
                            size_t line_no, const char *msg, size_t msg_len) {
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
