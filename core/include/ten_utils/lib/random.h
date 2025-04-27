//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

/**
 * @brief Generate random buffer with given size.
 * @param buf The buffer to store random data.
 * @param size The size of the buffer.
 * @return 0 on success, -1 on failure.
 *
 * This function generates random data. It may fail.
 */
TEN_UTILS_API int ten_random(void *buf, size_t size);

/**
 * @brief Generate random int n that start <= n < end
 * @param start The begin of random.
 * @param end The end of random.
 * @return A random int number
 */
TEN_UTILS_API int ten_random_int(int start, int end);

/**
 * @brief Generate random printable string with given size.
 * @param buf The buffer to store random data.
 * @param size The size of the buffer.
 * @return 0 on success, -1 on failure.
 */
TEN_UTILS_API int ten_random_string(char *buf, size_t size);

/**
 * @brief Generate random hex string with given size.
 * @param buf The buffer to store random data.
 * @param size The size of the buffer.
 */
TEN_UTILS_API int ten_random_hex_string(char *buf, size_t size);

/**
 * @brief Generate random base64 string with given size.
 * @param buf The buffer to store random data.
 * @param size The size of the buffer.
 */
TEN_UTILS_API int ten_random_base64_string(char *buf, size_t size);

/**
 * @brief Generate UUID string.
 * @param buf The buffer to store random data.
 * @param size The size of the buffer.
 */
TEN_UTILS_API int ten_uuid_new(char *buf, size_t size);
