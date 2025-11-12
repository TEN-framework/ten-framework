//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <mutex>
#include <string>

class GTestLog {
 public:
  static void print(const char *fmt, ...);
  static void log_to_file(const char *file, const char *fmt, ...);

 private:
  static std::mutex lock_;
};

#define AGO_LOG GTestLog::print
#define AGO_RECORD GTestLog::log_to_file

static std::string ToHex(const uint8_t *data, size_t size) {
  char buf[3] = {0};
  std::string s;
  for (int i = 0; i < size; i++) {
    snprintf(buf, 3, "%.2x", data[i]);
    s += buf;
  }
  return s;
}
