//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef int64_t ten_pid_t;

/**
 * @brief Get process id.
 */
TEN_UTILS_API ten_pid_t ten_task_get_id();
