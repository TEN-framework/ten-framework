//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/log/level.h"

#include "ten_utils/macro/check.h"

char ten_log_level_char(const TEN_LOG_LEVEL level) {
  switch (level) {
  case TEN_LOG_LEVEL_VERBOSE:
    return 'V';
  case TEN_LOG_LEVEL_DEBUG:
    return 'D';
  case TEN_LOG_LEVEL_INFO:
    return 'I';
  case TEN_LOG_LEVEL_WARN:
    return 'W';
  case TEN_LOG_LEVEL_ERROR:
    return 'E';
  case TEN_LOG_LEVEL_FATAL:
    return 'F';
  default:
    return '?';
  }
}

void ten_log_set_output_level(ten_log_t *self, TEN_LOG_LEVEL level) {
  TEN_ASSERT(self, "Invalid argument.");

  self->output_level = level;
}
