//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/time.h"

#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "include_internal/ten_utils/lib/time_cache.h"
#include "ten_utils/macro/check.h"

#if defined(OS_LINUX)
#include <linux/limits.h>
#elif defined(OS_MACOS)
#include <sys/syslimits.h>
#endif

int64_t ten_current_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // convert tv_sec & tv_usec to millisecond
  return (((int64_t)tv.tv_sec) * 1000) + (((int64_t)tv.tv_usec) / 1000);
}

int64_t ten_current_time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // convert tv_sec & tv_usec to microseconds
  return (((int64_t)tv.tv_sec) * 1000000) + (int64_t)tv.tv_usec;
}

void ten_current_time_info(struct tm *time_info, size_t *msec) {
  TEN_ASSERT(time_info && msec, "Invalid argument.");

  struct timeval tv;
  gettimeofday(&tv, 0);

  if (!ten_time_cache_get(&tv, time_info)) {
    (void)localtime_r(&tv.tv_sec, time_info);
    ten_time_cache_set(&tv, time_info);
  }

  *msec = (unsigned)tv.tv_usec / 1000;
}

// Sleep for the requested number of milliseconds.
void ten_sleep_ms(const int64_t msec) {
  struct timespec ts;
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  int res = 0;
  do {
    res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);
}

void ten_sleep_us(int64_t usec) { usleep(usec); }
