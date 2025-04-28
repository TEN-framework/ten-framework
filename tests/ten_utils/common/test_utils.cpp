//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "test_utils.h"

#include <stdio.h>

#include <mutex>

#if defined(__ANDROID__)
#include <android/log.h>

const char *LOG_TAG = "utils";
#endif

std::mutex GTestLog::lock_;

void GTestLog::print(const char *fmt, ...) {
  std::unique_lock<std::mutex> _(lock_);

  bool end_with_lf = false;

  if (fmt) {
    const char *newline = strrchr(fmt, '\n');
    end_with_lf = (newline > fmt && (newline - fmt) == strlen(fmt));
  }

  va_list ap;
  printf("[    LOG   ] ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
#if defined(__ANDROID__)
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, ap);
#endif
  va_end(ap);
  if (!end_with_lf) {
    printf("\n");
  }
  fflush(stdout);
}

void GTestLog::log_to_file(const char *file, const char *fmt, ...) {
  std::unique_lock<std::mutex> _(lock_);

  bool end_with_lf = false;

  if (fmt) {
    const char *newline = strrchr(fmt, '\n');
    end_with_lf = (newline > fmt && (newline - fmt) == strlen(fmt));
  }
  FILE *f = fopen(file, "ab+");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  if (!end_with_lf) {
    fprintf(f, "\n");
  }
  fflush(f);
  fclose(f);
}
