//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/lib/time.h"

#include <stdlib.h>
#include <time.h>

#include "ten_utils/lib/random.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/time.h"

void ten_random_sleep_range_ms(const int64_t min_msec, const int64_t max_msec) {
  // Check whether `min_msec` and `max_msec` are valid.
  TEN_ASSERT(min_msec >= 0 && max_msec > 0,
             "min_msec and max_msec must be greater than 0");
  TEN_ASSERT(min_msec <= max_msec,
             "min_msec must be less than or equal to max_msec");

  int64_t wait_time = 0;
  // Check the return value of `ten_random`.
  if (ten_random(&wait_time, sizeof(wait_time)) != 0) {
    ten_sleep_ms(0);
    return;
  }

  // ten_random might give us a negative number, so we need to take an action to
  // ensure it is positive.
  wait_time = llabs(wait_time);

  wait_time %= max_msec;
  wait_time += min_msec;

  ten_sleep_ms(wait_time);
}

void ten_string_append_time_info(ten_string_t *str, struct tm *time_info,
                                 size_t msec) {
  TEN_ASSERT(str, "Invalid argument.");

  ten_string_reserve(str, TIME_INFO_STRING_LEN);

  // Format the date and time into the buffer, excluding milliseconds.
  size_t written = strftime(&str->buf[str->first_unused_idx],
                            str->buf_size - str->first_unused_idx,
                            "%m-%d %H:%M:%S", time_info);
  TEN_ASSERT(written, "Should not happen.");

  str->first_unused_idx = strlen(str->buf);

  ten_string_append_formatted(str, ".%03zu", msec);
}
