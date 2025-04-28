//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/macro/check.h"

#define TEN_DO_WITH_MUTEX_LOCK(lock, blocks)                  \
  do {                                                        \
    int rc = ten_mutex_lock(lock);                            \
    TEN_ASSERT(!rc, "Unable to lock, error code: %d.", rc);   \
                                                              \
    {blocks}                                                  \
                                                              \
    rc = ten_mutex_unlock(lock);                              \
    TEN_ASSERT(!rc, "Unable to unlock, error code: %d.", rc); \
  } while (0)

typedef struct ten_mutex_t ten_mutex_t;

/**
 * @brief Create a mutex.
 * @return The mutex handle.
 */
TEN_UTILS_API ten_mutex_t *ten_mutex_create(void);

/**
 * @brief Lock a mutex.
 * @param mutex The mutex handle.
 * @return 0 if success, otherwise failed.
 *
 * @note This function will block until the mutex is unlocked.
 */
TEN_UTILS_API int ten_mutex_lock(ten_mutex_t *mutex);

/**
 * @brief Unlock a mutex.
 * @param mutex The mutex handle.
 * @return 0 if success, otherwise failed.
 */
TEN_UTILS_API int ten_mutex_unlock(ten_mutex_t *mutex);

/**
 * @brief Destroy a mutex.
 * @param mutex The mutex handle.
 */
TEN_UTILS_API void ten_mutex_destroy(ten_mutex_t *mutex);

/**
 * @brief Get system mutex handle.
 * @param mutex The mutex handle.
 * @return The system mutex handle.
 */
TEN_UTILS_API void *ten_mutex_get_native_handle(ten_mutex_t *mutex);
