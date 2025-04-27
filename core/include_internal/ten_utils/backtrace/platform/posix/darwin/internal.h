//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/common.h"

/**
 * @note On Mac, we are currently using a simple method instead of a complicated
 * posix method to dump backtrace. So we only need a field of
 * 'ten_backtrace_common_t'.
 */
typedef struct ten_backtrace_mac_t {
  ten_backtrace_common_t common;
} ten_backtrace_mac_t;
