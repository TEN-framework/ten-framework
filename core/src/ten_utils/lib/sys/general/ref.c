//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/ref.h"

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

/**
 * @brief Validates the integrity of a reference counter object.
 *
 * @param self Pointer to the reference counter object to check.
 * @param has_positive_ref_cnt If true, verifies that ref_cnt > 0; if false,
 * allows ref_cnt = 0.
 * @return true if the reference counter is valid, false otherwise.
 */
static bool ten_ref_check_integrity(ten_ref_t *self,
                                    bool has_positive_ref_cnt) {
  if (!self) {
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (ten_signature_get(&self->signature) != TEN_REF_SIGNATURE) {
    return false;
  }

  if (!self->on_end_of_life) {
    return false;
  }

  if (!self->supervisee) {
    return false;
  }

  // The reference count should be always > 0 before ten_ref_deinit().
  size_t minimum_ref_cnt = has_positive_ref_cnt ? 1 : 0;
  if (ten_atomic_load(&self->ref_cnt) < minimum_ref_cnt) {
    return false;
  }

  return true;
}

/**
 * @brief Deinitializes a reference counter object, clearing its internal state.
 *
 * This function resets all fields to invalid values to ensure that any further
 * integrity checks will fail. It should be called when the reference count
 * reaches zero and the object is being cleaned up.
 *
 * @param self Pointer to the reference counter object to deinitialize.
 */
void ten_ref_deinit(ten_ref_t *self) {
  TEN_ASSERT(self && ten_ref_check_integrity(
                         self, /* The reference count is zero now. */ false),
             "Invalid argument.");

  // Reset the member fields, so further integrity checking would be failed.
  ten_signature_set(&self->signature, 0);
  ten_atomic_store(&self->ref_cnt, 0);
  self->supervisee = NULL;
  self->on_end_of_life = NULL;
}

/**
 * @brief Destroys a reference counter object by deinitializing it and freeing
 * its memory.
 *
 * This function should only be called when the reference count has reached zero
 * and the object is no longer needed.
 *
 * @param self Pointer to the reference counter object to destroy.
 */
void ten_ref_destroy(ten_ref_t *self) {
  TEN_ASSERT(self && ten_ref_check_integrity(
                         self, /* The reference count is zero now. */ false),
             "Invalid argument.");

  ten_ref_deinit(self);

  TEN_FREE(self);
}

/**
 * @brief Initializes a reference counter object with the given supervisee and
 * callback.
 *
 * Sets up the reference counter with an initial count of 1 and associates it
 * with the object it will manage (supervisee).
 *
 * @param self Pointer to the reference counter object to initialize.
 * @param supervisee Pointer to the object being reference-counted.
 * @param on_end_of_life Callback function that will be invoked when the
 * reference count reaches zero. IMPORTANT: This callback MUST ALWAYS either:
 *   - Call ten_ref_destroy() if the ten_ref_t was allocated separately, OR
 *   - Call ten_ref_deinit() if the ten_ref_t is embedded in the supervisee.
 *     Failure to do so will result in a memory leak!
 */
void ten_ref_init(ten_ref_t *self, void *supervisee,
                  ten_ref_on_end_of_life_func_t on_end_of_life) {
  TEN_ASSERT(self, "Reference counter object cannot be NULL.");
  TEN_ASSERT(supervisee, "ten_ref_t needs to manage an object.");
  TEN_ASSERT(on_end_of_life, "The 'on_end_of_life' must be specified.");

  // Ensure the signature is invalid before initialization, to avoid
  // reinitialization.
  if (ten_signature_get(&self->signature) == TEN_REF_SIGNATURE) {
    TEN_LOGE(
        "Attempting to initialize a reference counter that is already "
        "initialized.");
    TEN_ASSERT(0, "Double initialization of reference counter.");
    return;
  }

  ten_signature_set(&self->signature, TEN_REF_SIGNATURE);
  self->supervisee = supervisee;
  self->on_end_of_life = on_end_of_life;

  // The initial value of a ten_ref_t instance should be 1.
  ten_atomic_store(&self->ref_cnt, 1);
}

/**
 * @brief Creates and initializes a new reference counter object.
 *
 * Allocates memory for a new reference counter and initializes it with the
 * provided supervisee and callback function.
 *
 * @param supervisee Pointer to the object being reference-counted.
 * @param on_end_of_life Callback function that will be invoked when the
 * reference count reaches zero.
 * @return A newly allocated and initialized reference counter object.
 */
ten_ref_t *ten_ref_create(void *supervisee,
                          ten_ref_on_end_of_life_func_t on_end_of_life) {
  TEN_ASSERT(supervisee && on_end_of_life,
             "ten_ref_t needs to manage an object.");

  ten_ref_t *self = (ten_ref_t *)TEN_MALLOC(sizeof(ten_ref_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_ref_init(self, supervisee, on_end_of_life);

  return self;
}

/**
 * @brief Increments the reference count of an object atomically.
 *
 * Increases the reference count by 1 if the current count is greater than zero.
 * This operation is thread-safe.
 *
 * @param self Pointer to the reference counter object.
 * @return true if the reference count was successfully incremented, false if
 * the object is already dead.
 */
bool ten_ref_inc_ref(ten_ref_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_ref_check_integrity(self, true), "Invalid argument.");

  // Increment the reference count.
  int64_t old_ref_cnt = ten_atomic_inc_if_non_zero(&self->ref_cnt);
  if (old_ref_cnt == 0) {
    TEN_LOGE("Add a reference to an object that is already dead.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  return true;
}

/**
 * @brief Decrements the reference count of an object atomically.
 *
 * Decreases the reference count by 1 if the current count is greater than zero.
 * If the count reaches zero, the on_end_of_life callback is invoked to clean up
 * the supervised object. This operation is thread-safe.
 *
 * @param self Pointer to the reference counter object.
 * @return true if the reference count was successfully decremented, false if
 * the object is already dead.
 */
bool ten_ref_dec_ref(ten_ref_t *self) {
  TEN_ASSERT(self && ten_ref_check_integrity(self, true), "Invalid argument.");

  int64_t old_ref_cnt = ten_atomic_dec_if_non_zero(&self->ref_cnt);
  if (old_ref_cnt == 0) {
    TEN_LOGE("Delete a reference to an object that is already dead.");
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (old_ref_cnt == 1) {
    // Call the 'end-of-life' callback of the 'supervisee'.
    TEN_ASSERT(
        self->on_end_of_life,
        "The 'on_end_of_life' must be specified to destroy the supervisee.");

    self->on_end_of_life(self, self->supervisee);
  }

  return true;
}
