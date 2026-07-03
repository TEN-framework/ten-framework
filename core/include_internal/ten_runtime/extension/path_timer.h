//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdint.h>

#define TEN_DEFAULT_PATH_CHECK_INTERVAL \
  ((uint64_t)(10 * 1000 * 1000))  // 10s by default.
#define TEN_DEFAULT_PATH_TIMEOUT \
  ((uint64_t)(3 * 60 * 1000 * 1000))  // 3min by default.

typedef struct ten_extension_t ten_extension_t;
typedef struct ten_timer_t ten_timer_t;
typedef struct ten_path_t ten_path_t;

typedef struct ten_path_timeout_info {
  uint64_t in_path_timeout;   // microseconds.
  uint64_t out_path_timeout;  // microseconds.
  uint64_t check_interval;    // microseconds.
} ten_path_timeout_info;

TEN_RUNTIME_PRIVATE_API ten_timer_t *ten_extension_create_timer_for_in_path(
    ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API ten_timer_t *ten_extension_create_timer_for_out_path(
    ten_extension_t *self);
