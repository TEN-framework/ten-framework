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

#define NORMALIZE_PATH_BUF_SIZE 4096

/**
 * @brief Normalizes a file path by resolving '..' and '.' path components.
 *
 * @param path The input path to normalize
 * @param normalized_path Buffer to receive the normalized path
 * @param buffer_size Size of the normalized_path buffer
 * @return true on success, false if buffer is too small or path is invalid
 */
TEN_UTILS_API bool ten_backtrace_normalize_path(const char *path,
                                                char *normalized_path,
                                                size_t buffer_size);
