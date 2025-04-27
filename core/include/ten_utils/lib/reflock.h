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
#include "ten_utils/lib/event.h"

typedef enum REFLOCK_FLAG {
  TEN_REFLOCK_REF = (int64_t)0x00000001,
  TEN_REFLOCK_DESTROY = (int64_t)0x10000000,
  TEN_REFLOCK_POISON = (int64_t)0x300dead0,
  TEN_REFLOCK_DESTROY_MASK = (int64_t)0xf0000000,
  TEN_REFLOCK_REF_MASK = (int64_t)0x0fffffff,
} REFLOCK_FLAG;

typedef struct ten_reflock_t {
  ten_atomic_t state;
  ten_event_t *event;
} ten_reflock_t;

/**
 * @brief Initialize a reflock.
 * @param reflock The reflock to initialize.
 */
TEN_UTILS_API void ten_reflock_init(ten_reflock_t *lock);

/**
 * @brief Increase the reference count of a reflock.
 * @param reflock The reflock to increase the reference count.
 */
TEN_UTILS_API void ten_reflock_ref(ten_reflock_t *lock);

/**
 * @brief Decrease the reference count of a reflock.
 * @param reflock The reflock to decrease the reference count.
 */
TEN_UTILS_API void ten_reflock_unref(ten_reflock_t *lock);

/**
 * @brief Decrease reference count and destroy after it's zero.
 * @param reflock The reflock to decrease the reference count.
 * @note This function will wait until the reflock is zero
 */
TEN_UTILS_API void ten_reflock_unref_destroy(ten_reflock_t *lock);
