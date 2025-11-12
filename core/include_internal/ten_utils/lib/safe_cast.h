//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <limits.h>   // for INT_MAX, LONG_MAX
#include <stdbool.h>  // for bool
#include <stddef.h>   // for size_t
#include <stdint.h>   // for INT32_MAX

static inline bool safe_cast_size_t_to_int(size_t in, int *out) {
  if (in <= (size_t)INT_MAX) {
    *out = (int)in;
    return true;
  }
  return false;
}

static inline bool safe_cast_size_t_to_long(size_t in, long *out) {
  if (in <= (size_t)LONG_MAX) {
    *out = (long)in;
    return true;
  }
  return false;
}

static inline bool safe_cast_size_t_to_int32(size_t in, int32_t *out) {
  if (in <= (size_t)INT32_MAX) {
    *out = (int32_t)in;
    return true;
  }
  return false;
}
