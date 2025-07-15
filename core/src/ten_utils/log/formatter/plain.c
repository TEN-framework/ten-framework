//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include <inttypes.h>
#include <time.h>

#include "include_internal/ten_utils/lib/safe_cast.h"
#include "include_internal/ten_utils/lib/time.h"
#include "include_internal/ten_utils/log/formatter.h"
#include "include_internal/ten_utils/log/level.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/pid.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

void ten_log_plain_formatter(ten_string_t *buf, TEN_LOG_LEVEL level,
                             const char *func_name, size_t func_name_len,
                             const char *file_name, size_t file_name_len,
                             size_t line_no, const char *msg, size_t msg_len) {
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
