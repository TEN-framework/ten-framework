//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Opens a file for reading with proper error handling.
 */
TEN_UTILS_API int ten_backtrace_open_file(const char *filename,
                                          bool *does_not_exist);

/**
 * @brief Closes a file descriptor with error checking.
 */
TEN_UTILS_API bool ten_backtrace_close_file(int fd);
