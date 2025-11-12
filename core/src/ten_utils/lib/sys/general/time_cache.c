//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/lib/time_cache.h"

#include <stdbool.h>
#include <time.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/macro/check.h"

// There is no 'struct timeval' in Windows.
#if !defined(OS_WINDOWS)

// Indicates that the time cache is outdated.
#define TIME_CACHE_STALE (0x40000000)

// Indicate the time is updating.
#define TIME_CACHE_UPDATING (0x40000000 | 0x80000000)

// The initial value is `TIME_CACHE_STALE`, indicating that the cache is
// outdated.
static ten_atomic_t g_time_cache_mode = TIME_CACHE_STALE;

// Store the values of the time cache. g_time_cache_tv stores seconds and
// microseconds.
static struct timeval g_time_cache_tv = {0, 0};

// Store the values of the time cache. g_time_cache_tm stores the decomposed
// time structure.
static struct tm g_time_cache_tm;

bool ten_time_cache_get(const struct timeval *tv, struct tm *tm) {
  TEN_ASSERT(tv && tm, "Invalid argument.");

  int64_t mode = ten_atomic_load(&g_time_cache_mode);
  if (0 == (mode & TIME_CACHE_UPDATING)) {
    // The time cache is up-to-date, so the reader can use 'tm' directly.
    //
    // And add 1 to indicate that there is one more 'reader', and this operation
    // will prevent the 'writer' to update the time.
    mode = ten_atomic_fetch_add(&g_time_cache_mode, 1);
    if (0 == (mode & TIME_CACHE_UPDATING)) {
      // The time cache is up-to-date, so the reader can use 'tm' directly.

      if (g_time_cache_tv.tv_sec == tv->tv_sec) {
        // Use the cached 'tm' directly.
        *tm = g_time_cache_tm;

        // Indicate that one 'reader' is completed.
        ten_atomic_sub_fetch(&g_time_cache_mode, 1);
        return true;
      }

      // Mark the time cache is outdated.
      ten_atomic_or_fetch(&g_time_cache_mode, TIME_CACHE_STALE);
    }

    // The time cache is _not_ up-to-date, so the reader can not use 'tm'
    // directly.
    ten_atomic_sub_fetch(&g_time_cache_mode, 1);
  }

  // The time cache is _not_ up-to-date, so the reader can not use 'tm'
  // directly.
  return false;
}

void ten_time_cache_set(const struct timeval *tv, struct tm *tm) {
  TEN_ASSERT(tv && tm, "Invalid argument.");

  // Only update the time when the time cache is outdated.
  int64_t stale = TIME_CACHE_STALE;

  // Mark the time is updating, and create a critical section for the updating
  // operation.
  if (ten_atomic_bool_compare_swap(&g_time_cache_mode, stale,
                                   TIME_CACHE_UPDATING)) {
    g_time_cache_tv = *tv;
    g_time_cache_tm = *tm;

    // Mark the time cache is up-to-date.
    ten_atomic_and_fetch(&g_time_cache_mode, ~TIME_CACHE_UPDATING);
  }
}

#endif
