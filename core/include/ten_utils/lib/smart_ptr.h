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
#include "ten_utils/sanitizer/thread_check.h"

/**
 * @brief The internal architecture of a shared_ptr/weak_ptr is as follows.
 *
 * shared_ptr \
 * shared_ptr -> ctrl_blk -> data
 *   weak_ptr /
 *
 * A shared_ptr would contribute 1 'shared_cnt' (and additional 1 'weak_cnt' for
 * the 1st shared_ptr), and a weak_ptr would contribute 1 'weak_cnt' only.
 */

typedef enum TEN_SMART_PTR_TYPE {
  TEN_SMART_PTR_SHARED,
  TEN_SMART_PTR_WEAK,
} TEN_SMART_PTR_TYPE;

typedef struct ten_smart_ptr_ctrl_blk_t {
  ten_atomic_t shared_cnt;
  ten_atomic_t weak_cnt;

  void *data;  // Points to the shared data.
  void (*destroy)(void *data);
} ten_smart_ptr_ctrl_blk_t;

typedef struct ten_smart_ptr_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_SMART_PTR_TYPE type;

  // Control the life of the memory space pointed by this ten_smart_ptr_t
  // object.
  ten_smart_ptr_ctrl_blk_t *ctrl_blk;
} ten_smart_ptr_t;

typedef ten_smart_ptr_t ten_shared_ptr_t;
typedef ten_smart_ptr_t ten_weak_ptr_t;

typedef bool (*ten_smart_ptr_type_checker)(void *data);

// @{
// Smart pointer

TEN_UTILS_PRIVATE_API ten_smart_ptr_t *ten_smart_ptr_clone(
    ten_smart_ptr_t *other);

/**
 * @brief Destroy a smart_ptr.
 */
TEN_UTILS_PRIVATE_API void ten_smart_ptr_destroy(ten_smart_ptr_t *self);

/**
 * @brief This function must be used with caution. Essentially, this function
 * can only be used within the TEN runtime and should not be accessed
 * externally. This is because if the parameter is actually a weak_ptr, the
 * function expects the weak_ptr to remain valid after the completion of the
 * function, meaning the object pointed to by the weak_ptr remains valid after
 * the function ends.
 */
TEN_UTILS_API void *ten_smart_ptr_get_data(ten_smart_ptr_t *self);

TEN_UTILS_API bool ten_smart_ptr_check_type(
    ten_smart_ptr_t *self, ten_smart_ptr_type_checker type_checker);

// @}

// @{
// Shared pointer

#if defined(__cplusplus)
#define ten_shared_ptr_create(ptr, destroy) \
  ten_shared_ptr_create_(ptr, reinterpret_cast<void (*)(void *)>(destroy))
#else
#define ten_shared_ptr_create(ptr, destroy) \
  ten_shared_ptr_create_(ptr, (void (*)(void *))(destroy))
#endif

TEN_UTILS_API ten_shared_ptr_t *ten_shared_ptr_create_(void *ptr,
                                                       void (*destroy)(void *));

TEN_UTILS_API ten_shared_ptr_t *ten_shared_ptr_clone(ten_shared_ptr_t *other);

TEN_UTILS_API void ten_shared_ptr_destroy(ten_shared_ptr_t *self);

/**
 * @brief Get the pointing resource.
 */
TEN_UTILS_API void *ten_shared_ptr_get_data(ten_shared_ptr_t *self);

// @}

// @{
// Weak pointer

/**
 * @brief Create a weak_ptr from a shared_ptr.
 *
 * @note This function expects that @a shared_ptr is valid.
 */
TEN_UTILS_API ten_weak_ptr_t *ten_weak_ptr_create(ten_shared_ptr_t *shared_ptr);

TEN_UTILS_API ten_weak_ptr_t *ten_weak_ptr_clone(ten_weak_ptr_t *other);

TEN_UTILS_API void ten_weak_ptr_destroy(ten_weak_ptr_t *self);

/**
 * @brief Convert a weak pointer into a shared pointer if the pointing
 * resource is still available.
 */
TEN_UTILS_API ten_shared_ptr_t *ten_weak_ptr_lock(ten_weak_ptr_t *self);

// @}
