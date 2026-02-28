//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

typedef struct ten_process_mutex_t ten_process_mutex_t;

/**
 * @brief Create a mutex.
 * @return The mutex handle.
 */
TEN_UTILS_API ten_process_mutex_t *ten_process_mutex_create(const char *name);

/**
 * @brief Lock a mutex.
 * @param mutex The mutex handle.
 * @return 0 if success, otherwise failed.
 *
 * @note This function will block until the mutex is unlocked.
 */
TEN_UTILS_API int ten_process_mutex_lock(ten_process_mutex_t *mutex);

/**
 * @brief Unlock a mutex.
 * @param mutex The mutex handle.
 * @return 0 if success, otherwise failed.
 */
TEN_UTILS_API int ten_process_mutex_unlock(ten_process_mutex_t *mutex);

/**
 * @brief Destroy a mutex.
 * @param mutex The mutex handle.
 */
TEN_UTILS_API void ten_process_mutex_destroy(ten_process_mutex_t *mutex);
