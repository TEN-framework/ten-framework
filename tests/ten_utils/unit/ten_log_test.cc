//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_utils/log/level.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/log/output.h"

TEST(LogTest, FileOutput1) {  // NOLINT
  ten_log_t log;
  ten_log_init(&log);

  ten_log_set_output_level(&log, TEN_LOG_LEVEL_ERROR);
  ten_log_set_output_to_file(&log, "test1.log");

  TEN_LOGE_AUX(&log, "test %s test", "hello");

  ten_log_deinit(&log);
}

TEST(LogTest, FileOutputReload) {  // NOLINT
  ten_log_t log;
  ten_log_init(&log);

  ten_log_set_output_level(&log, TEN_LOG_LEVEL_ERROR);
  ten_log_set_output_to_file(&log, "test1.log");

  TEN_LOGE_AUX(&log, "test %s test", "hello");

  ten_log_reload(&log);

  TEN_LOGE_AUX(&log, "test %s test", "hello after reload");

  ten_log_deinit(&log);
}
