//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/task.h"
#include "ten_utils/lib/thread.h"

/**
 * This is actually a "busy loop" with "pause" instruction.
 * It's not possible to implement a "real" spin lock in userspace because
 * you have no way to disable thread schedule and interrupts.
 */

typedef struct ten_spinlock_t {
  ten_atomic_t lock;
} ten_spinlock_t;

#define TEN_SPINLOCK_INIT {0}

/**
 * @brief Initialize a spinlock.
 */
TEN_UTILS_API void ten_spinlock_init(ten_spinlock_t *spin);

/**
 * @brief Initialize a spinlock from address
 * @note If |addr| exists in a shared memory, this lock can be used as IPC lock
 */
TEN_UTILS_API ten_spinlock_t *ten_spinlock_from_addr(ten_atomic_t *addr);

/**
 * @brief Acquire a spinlock.
 * @note This function will block if the lock is held by others. Recursively
 *       acquire the same lock will result in dead lock
 */
TEN_UTILS_API void ten_spinlock_lock(ten_spinlock_t *spin);

/**
 * @brief Try to acquire a spinlock.
 * @return 0 if the lock is acquired, -1 otherwise.
 */
TEN_UTILS_API int ten_spinlock_trylock(ten_spinlock_t *spin);

/**
 * @brief Release a spinlock.
 */
TEN_UTILS_API void ten_spinlock_unlock(ten_spinlock_t *spin);

typedef struct ten_recursive_spinlock_t {
  ten_spinlock_t lock;
  volatile ten_pid_t pid;
  volatile ten_tid_t tid;
  volatile int64_t count;
} ten_recursive_spinlock_t;

#define TEN_RECURSIVE_SPINLOCK_INIT {TEN_SPINLOCK_INIT, -1, -1, 0}

/**
 * @brief Initialize a recursive spinlock
 */
TEN_UTILS_API void ten_recursive_spinlock_init(ten_recursive_spinlock_t *spin);

/**
 * @brief Initialize a recursive spinlock from address
 * @note If |addr| exists in a shared memory, this lock can be used as IPC lock
 */
TEN_UTILS_API ten_recursive_spinlock_t *ten_recursive_spinlock_from_addr(
    uint8_t addr[sizeof(ten_recursive_spinlock_t)]);

/**
 * @brief Acquire a recursive spinlock.
 * @note This function will block if the lock is held by another thread.
 */
TEN_UTILS_API void ten_recursive_spinlock_lock(ten_recursive_spinlock_t *spin);

/**
 * @brief Try to acquire a recursive spinlock.
 * @return 0 if the lock is acquired, -1 otherwise.
 */
TEN_UTILS_API int ten_recursive_spinlock_trylock(
    ten_recursive_spinlock_t *spin);

/**
 * @brief Release a recursive spinlock.
 */
TEN_UTILS_API void ten_recursive_spinlock_unlock(
    ten_recursive_spinlock_t *spin);
