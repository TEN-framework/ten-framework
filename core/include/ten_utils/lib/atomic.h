//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef int64_t ten_atomic_t;

/**
 * @brief Atomically loads the value of an atomic variable.
 * @param a Pointer to the atomic variable to load from.
 * @return The current value of the atomic variable.
 */
TEN_UTILS_API int64_t ten_atomic_load(volatile ten_atomic_t *a);

/**
 * @brief Atomically stores a value into an atomic variable.
 * @param a Pointer to the atomic variable to store into.
 * @param v The value to store.
 */
TEN_UTILS_API void ten_atomic_store(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically adds a value to an atomic variable and returns the previous
 * value.
 * @param a Pointer to the atomic variable.
 * @param v The value to add.
 * @return The value of the atomic variable before the addition.
 */
TEN_UTILS_API int64_t ten_atomic_fetch_add(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically adds a value to an atomic variable and returns the
 * resulting value.
 * @param a Pointer to the atomic variable.
 * @param v The value to add.
 * @return The value of the atomic variable after the addition.
 */
TEN_UTILS_API int64_t ten_atomic_add_fetch(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically performs a bitwise AND operation and returns the resulting
 * value.
 * @param a Pointer to the atomic variable.
 * @param v The value to AND with.
 * @return The value of the atomic variable after the AND operation.
 */
TEN_UTILS_API int64_t ten_atomic_and_fetch(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically subtracts a value from an atomic variable and returns the
 * previous value.
 * @param a Pointer to the atomic variable.
 * @param v The value to subtract.
 * @return The value of the atomic variable before the subtraction.
 */
TEN_UTILS_API int64_t ten_atomic_fetch_sub(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically subtracts a value from an atomic variable and returns the
 * resulting value.
 * @param a Pointer to the atomic variable.
 * @param v The value to subtract.
 * @return The value of the atomic variable after the subtraction.
 */
TEN_UTILS_API int64_t ten_atomic_sub_fetch(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically performs a bitwise OR operation and returns the resulting
 * value.
 * @param a Pointer to the atomic variable.
 * @param v The value to OR with.
 * @return The value of the atomic variable after the OR operation.
 */
TEN_UTILS_API int64_t ten_atomic_or_fetch(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically performs a bitwise AND operation and returns the previous
 * value.
 * @param a Pointer to the atomic variable.
 * @param v The value to AND with.
 * @return The value of the atomic variable before the AND operation.
 */
TEN_UTILS_API int64_t ten_atomic_fetch_and(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically performs a bitwise OR operation and returns the previous
 * value.
 * @param a Pointer to the atomic variable.
 * @param v The value to OR with.
 * @return The value of the atomic variable before the OR operation.
 */
TEN_UTILS_API int64_t ten_atomic_fetch_or(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically exchanges the value of an atomic variable with a new value.
 * @param a Pointer to the atomic variable.
 * @param v The new value to set.
 * @return The value of the atomic variable before the exchange.
 */
TEN_UTILS_API int64_t ten_atomic_test_set(volatile ten_atomic_t *a, int64_t v);

/**
 * @brief Atomically compares the value of an atomic variable with an expected
 * value, and if they match, replaces it with a new value.
 * @param a Pointer to the atomic variable.
 * @param comp The expected value to compare against.
 * @param xchg The new value to set if the comparison succeeds.
 * @return The original value of the atomic variable regardless of whether the
 * swap occurred.
 */
TEN_UTILS_API int64_t ten_atomic_val_compare_swap(volatile ten_atomic_t *a,
                                                  int64_t comp, int64_t xchg);

/**
 * @brief Atomically compares the value of an atomic variable with an expected
 * value, and if they match, replaces it with a new value.
 * @param a Pointer to the atomic variable.
 * @param comp The expected value to compare against.
 * @param xchg The new value to set if the comparison succeeds.
 * @return 1 (true) if the swap occurred because the values matched, 0 (false)
 * otherwise.
 */
TEN_UTILS_API int ten_atomic_bool_compare_swap(volatile ten_atomic_t *a,
                                               int64_t comp, int64_t xchg);

/**
 * @brief Atomically increments an atomic variable by 1, but only if its current
 * value is not zero.
 * @param a Pointer to the atomic variable.
 * @return The original value of the atomic variable before the operation.
 *         If the original value was 0, no increment occurs.
 */
TEN_UTILS_API int64_t ten_atomic_inc_if_non_zero(volatile ten_atomic_t *a);

/**
 * @brief Atomically decrements an atomic variable by 1, but only if its current
 * value is not zero.
 * @param a Pointer to the atomic variable.
 * @return The original value of the atomic variable before the operation.
 *         If the original value was 0, no decrement occurs.
 */
TEN_UTILS_API int64_t ten_atomic_dec_if_non_zero(volatile ten_atomic_t *a);

/**
 * @brief Inserts a full memory barrier, ensuring all memory operations before
 * this call complete before any memory operations after this call.
 */
TEN_UTILS_API void ten_memory_barrier(void);

#if defined(_WIN32)
#include <intrin.h>
#define ten_compiler_barrier() _ReadWriteBarrier()
#else
#define ten_compiler_barrier()       \
  do {                               \
    asm volatile("" : : : "memory"); \
  } while (0)
#endif
