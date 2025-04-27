//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"

#define TEN_REF_SIGNATURE 0x759D8D9D2661E36BU

typedef struct ten_ref_t ten_ref_t;

/**
 * @brief Callback function type that is invoked when an object's reference
 * count reaches zero.
 *
 * IMPORTANT: This function MUST be thread-safe as it may be called from
 * any thread that calls `ten_ref_dec_ref()`.
 *
 * @param ref Pointer to the reference counter object.
 * @param supervisee Pointer to the object being reference-counted.
 */
typedef void (*ten_ref_on_end_of_life_func_t)(ten_ref_t *ref, void *supervisee);

/**
 * @brief Reference counting structure to manage object lifetimes.
 */
typedef struct ten_ref_t {
  ten_signature_t signature;

  ten_atomic_t ref_cnt;

  // The object which is managed by this 'ten_ref_t'. This field should _not_ be
  // modified after 'ten_ref_t' has been initted, therefore, we don't need to
  // care about its thread safety.
  void *supervisee;

  // This function will be called when the end-of-life of 'supervisee' is
  // reached. This field should _not_ be modified after 'ten_ref_t' has been
  // initted, therefore, we don't need to care about its thread safety.
  ten_ref_on_end_of_life_func_t on_end_of_life;
} ten_ref_t;

/**
 * @brief Creates a new reference counter object.
 *
 * @param supervisee Pointer to the object being reference-counted.
 * @param on_end_of_life Callback function that will be invoked when reference
 * count reaches zero. IMPORTANT: This callback must handle proper cleanup:
 *        - If the ten_ref_t is a standalone object (allocated separately), call
 * ten_ref_destroy() in this callback.
 *        - If the ten_ref_t is embedded within the supervisee, call
 * ten_ref_deinit() in this callback.
 *
 * @return A newly allocated reference counter object.
 */
TEN_UTILS_API ten_ref_t *ten_ref_create(
    void *supervisee, ten_ref_on_end_of_life_func_t on_end_of_life);

/**
 * @brief Forcibly destroys a reference counter object regardless of its current
 * count.
 *
 * @param self Pointer to the reference counter object.
 *
 * @note This function should be used with caution as it bypasses normal
 * reference counting. Only use when you need to immediately terminate an object
 * regardless of its references.
 */
TEN_UTILS_API void ten_ref_destroy(ten_ref_t *self);

/**
 * @brief Initializes an existing reference counter object.
 *
 * @param self Pointer to the reference counter object to initialize.
 * @param supervisee Pointer to the object being reference-counted.
 * @param on_end_of_life Callback function that will be invoked when reference
 * count reaches zero. IMPORTANT: This callback must handle proper cleanup:
 *        - If the ten_ref_t is a standalone object (allocated separately), call
 * ten_ref_destroy() in this callback.
 *        - If the ten_ref_t is embedded within the supervisee, call
 * ten_ref_deinit() in this callback.
 */
TEN_UTILS_API void ten_ref_init(ten_ref_t *self, void *supervisee,
                                ten_ref_on_end_of_life_func_t on_end_of_life);

/**
 * @brief Deinitializes a reference counter object.
 *
 * @param self Pointer to the reference counter object.
 */
TEN_UTILS_API void ten_ref_deinit(ten_ref_t *self);

/**
 * @brief Increments the reference count of an object.
 *
 * @param self Pointer to the reference counter object.
 * @return true if the reference count was successfully incremented, false
 * otherwise.
 */
TEN_UTILS_API bool ten_ref_inc_ref(ten_ref_t *self);

/**
 * @brief Decrements the reference count of an object.
 *
 * @param self Pointer to the reference counter object.
 * @return true if the reference count was successfully decremented, false
 * otherwise. If the count reaches zero, the on_end_of_life callback will be
 * invoked.
 */
TEN_UTILS_API bool ten_ref_dec_ref(ten_ref_t *self);
