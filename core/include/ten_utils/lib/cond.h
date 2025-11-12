//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef struct ten_cond_t ten_cond_t;
typedef struct ten_mutex_t ten_mutex_t;

/**
 * @brief Create a condition variable.
 */
TEN_UTILS_API ten_cond_t *ten_cond_create(void);

/**
 * @brief Destroy a condition variable.
 */
TEN_UTILS_API void ten_cond_destroy(ten_cond_t *cond);

/**
 * @brief Wait on a condition variable.
 * @param cond The condition variable to wait on.
 * @param mutex The mutex to unlock while waiting.
 * @param wait_ms The maximum time to wait in milliseconds.
 * @return 0 on success, -1 on error.
 *
 * @note This function will unlock the mutex before waiting and lock it again
 *       when it is signaled. Surprise wakeup still happens just like pthread
 *       version of condition variable.
 */
TEN_UTILS_API int ten_cond_wait(ten_cond_t *cond, ten_mutex_t *mutex,
                                int64_t wait_ms);

/**
 * @brief Wait on a condition variable while predicate() is true.
 * @param cond The condition variable to wait on.
 * @param mutex The mutex to unlock while waiting.
 * @param predicate The predicate to check.
 * @param arg The argument to pass to predicate().
 * @param wait_ms The maximum time to wait in milliseconds.
 * @return 0 on success, -1 on error.
 *
 * @note This function will unlock the mutex before waiting and lock it again
 *       when it is signaled. Surprise wakeup does _not_ happen because we
 *       instantly check predicate() before leaving.
 */
TEN_UTILS_API int ten_cond_wait_while(ten_cond_t *cond, ten_mutex_t *mutex,
                                      int (*predicate)(void *), void *arg,
                                      int64_t wait_ms);

/**
 * @brief Signal a condition variable.
 * @param cond The condition variable to signal.
 * @return 0 on success, -1 on error.
 */
TEN_UTILS_API int ten_cond_signal(ten_cond_t *cond);

/**
 * @brief Broadcast a condition variable.
 * @param cond The condition variable to broadcast.
 * @return 0 on success, -1 on error.
 */
TEN_UTILS_API int ten_cond_broadcast(ten_cond_t *cond);
