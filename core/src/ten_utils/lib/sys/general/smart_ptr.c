//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/smart_ptr.h"

#include <inttypes.h>
#include <stddef.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"

#define TEN_SMART_PTR_SIGNATURE 0x7BB9769E3A5CBA5FU

#define TEN_SMART_PTR_COUNTER_RED_ZONE 10000

static bool ten_smart_ptr_check_integrity(ten_smart_ptr_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->ctrl_blk, "The control block should always be valid.");

  if (ten_signature_get(&self->signature) != TEN_SMART_PTR_SIGNATURE) {
    return false;
  }

  if (ten_atomic_load(&self->ctrl_blk->shared_cnt) >
      (INT64_MAX - TEN_SMART_PTR_COUNTER_RED_ZONE)) {
    // A hint to indicate that we need to consider the wrap back problem.
    return false;
  }

  if (ten_atomic_load(&self->ctrl_blk->weak_cnt) >
      (INT64_MAX - TEN_SMART_PTR_COUNTER_RED_ZONE)) {
    // A hint to indicate that we need to consider the wrap back problem.
    return false;
  }

  return true;
}

static bool ten_shared_ptr_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_smart_ptr_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->type == TEN_SMART_PTR_SHARED, "Invalid argument.");
  TEN_ASSERT(self->ctrl_blk->shared_cnt, "The shared_ref_cnt should not be 0.");

  return true;
}

static bool ten_weak_ptr_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_smart_ptr_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->type == TEN_SMART_PTR_WEAK, "Invalid argument.");
  TEN_ASSERT(self->ctrl_blk->weak_cnt, "The weak_ref_cnt should not be 0.");

  return true;
}

static void ten_smart_ptr_ctrl_blk_init(ten_smart_ptr_ctrl_blk_t *self,
                                        void *data,
                                        void (*destroy)(void *ptr)) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_atomic_store(&self->shared_cnt, 1);

  // A 'counter' structure will be created due to a shared_ptr, and the
  // existence of a shared_ptr will contribute 1 weak_ref_cnt. 'weak_ref_cnt'
  // would be used to determine if the control block ('counter') could be
  // deleted or not.
  ten_atomic_store(&self->weak_cnt, 1);

  self->data = data;
  self->destroy = destroy;
}

static ten_shared_ptr_t *ten_smart_ptr_create_without_ctrl_blk(
    TEN_SMART_PTR_TYPE type) {
  TEN_ASSERT((type == TEN_SMART_PTR_SHARED || type == TEN_SMART_PTR_WEAK),
             "Invalid argument.");

  ten_shared_ptr_t *self =
      (ten_shared_ptr_t *)TEN_MALLOC(sizeof(ten_shared_ptr_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_SMART_PTR_SIGNATURE);

  self->type = type;

  return self;
}

void *ten_smart_ptr_get_data(ten_smart_ptr_t *self) {
  TEN_ASSERT(self && ten_smart_ptr_check_integrity(self), "Invalid argument.");

  void *result = NULL;

  switch (self->type) {
  case TEN_SMART_PTR_SHARED:
    result = ten_shared_ptr_get_data(self);
    break;

  case TEN_SMART_PTR_WEAK: {
    ten_shared_ptr_t *shared_one = ten_weak_ptr_lock(self);
    TEN_ASSERT(shared_one, "Should not happen.");
    if (shared_one) {
      result = ten_shared_ptr_get_data(shared_one);
      ten_shared_ptr_destroy(shared_one);
    }
    break;
  }

  default:
    TEN_ASSERT(0, "Invalid argument.");
    break;
  }

  return result;
}

ten_smart_ptr_t *ten_smart_ptr_clone(ten_smart_ptr_t *other) {
  TEN_ASSERT(other, "Invalid argument.");
  TEN_ASSERT(ten_smart_ptr_check_integrity(other), "Invalid argument.");

  switch (other->type) {
  case TEN_SMART_PTR_SHARED:
    return ten_shared_ptr_clone(other);

  case TEN_SMART_PTR_WEAK:
    return ten_weak_ptr_clone(other);

  default:
    TEN_ASSERT(0, "Invalid argument.");
    return NULL;
  }
}

void ten_smart_ptr_destroy(ten_smart_ptr_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_smart_ptr_check_integrity(self), "Invalid argument.");

  switch (self->type) {
  case TEN_SMART_PTR_SHARED: {
    int64_t shared_cnt = ten_atomic_sub_fetch(&self->ctrl_blk->shared_cnt, 1);
    TEN_ASSERT(shared_cnt >= 0, "Should not happen.");

    if (shared_cnt == 0) {
      if (self->ctrl_blk->destroy) {
        self->ctrl_blk->destroy(self->ctrl_blk->data);
        self->ctrl_blk->data = NULL;
      }

      int64_t weak_cnt = ten_atomic_sub_fetch(&self->ctrl_blk->weak_cnt, 1);
      TEN_ASSERT(weak_cnt >= 0, "Should not happen.");

      if (weak_cnt == 0) {
        TEN_FREE(self->ctrl_blk);
        self->ctrl_blk = NULL;
      }
    }
    break;
  }
  case TEN_SMART_PTR_WEAK: {
    int64_t weak_cnt = ten_atomic_sub_fetch(&self->ctrl_blk->weak_cnt, 1);
    TEN_ASSERT(weak_cnt >= 0, "Should not happen.");

    if (weak_cnt == 0) {
      TEN_FREE(self->ctrl_blk);
      self->ctrl_blk = NULL;
    }
    break;
  }
  default:
    TEN_ASSERT(0 && "Should not happen.", "Invalid argument.");
    break;
  }

  TEN_FREE(self);
}

bool ten_smart_ptr_check_type(ten_smart_ptr_t *self,
                              ten_smart_ptr_type_checker type_checker) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_smart_ptr_check_integrity(self), "Invalid argument.");

  switch (self->type) {
  case TEN_SMART_PTR_SHARED:
    return type_checker(ten_shared_ptr_get_data(self));

  case TEN_SMART_PTR_WEAK: {
    ten_shared_ptr_t *shared_one = ten_weak_ptr_lock(self);
    if (shared_one) {
      bool rc = type_checker(ten_shared_ptr_get_data(shared_one));
      ten_shared_ptr_destroy(shared_one);
      return rc;
    } else {
      return false;
    }
  }

  default:
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }
}

ten_shared_ptr_t *ten_shared_ptr_create_(void *ptr,
                                         void (*destroy)(void *ptr)) {
  TEN_ASSERT(ptr, "Invalid argument.");

  ten_shared_ptr_t *self =
      ten_smart_ptr_create_without_ctrl_blk(TEN_SMART_PTR_SHARED);
  TEN_ASSERT(self, "Invalid argument.");

  ten_smart_ptr_ctrl_blk_t *ctrl_blk =
      (ten_smart_ptr_ctrl_blk_t *)TEN_MALLOC(sizeof(ten_smart_ptr_ctrl_blk_t));
  TEN_ASSERT(ctrl_blk, "Failed to allocate memory.");

  ten_smart_ptr_ctrl_blk_init(ctrl_blk, ptr, destroy);
  self->ctrl_blk = ctrl_blk;

  return self;
}

/**
 * @brief Clone a shared_ptr.
 *
 * @note This function expects @a other is valid during the execution of this
 * function.
 */
ten_shared_ptr_t *ten_shared_ptr_clone(ten_shared_ptr_t *other) {
  TEN_ASSERT(other, "Invalid argument.");
  TEN_ASSERT(ten_shared_ptr_check_integrity(other), "Invalid argument.");

  ten_shared_ptr_t *self =
      ten_smart_ptr_create_without_ctrl_blk(TEN_SMART_PTR_SHARED);
  TEN_ASSERT(self, "Invalid argument.");

  self->ctrl_blk = other->ctrl_blk;
  ten_atomic_add_fetch(&self->ctrl_blk->shared_cnt, 1);

  return self;
}

void ten_shared_ptr_destroy(ten_shared_ptr_t *self) {
  ten_smart_ptr_destroy(self);
}

void *ten_shared_ptr_get_data(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_shared_ptr_check_integrity(self), "Invalid argument.");

  TEN_ASSERT(self->ctrl_blk->data != NULL,
             "shared_ptr holds nothing: shared_ref_cnt(%" PRIx64
             "), weak_ref_cnt(%" PRIx64 ")",
             ten_atomic_load(&self->ctrl_blk->shared_cnt),
             ten_atomic_load(&self->ctrl_blk->weak_cnt));

  return self->ctrl_blk->data;
}

// Weak pointer

ten_weak_ptr_t *ten_weak_ptr_create(ten_shared_ptr_t *shared_ptr) {
  TEN_ASSERT(shared_ptr && ten_shared_ptr_check_integrity(shared_ptr),
             "Invalid argument.");

  ten_shared_ptr_t *self =
      ten_smart_ptr_create_without_ctrl_blk(TEN_SMART_PTR_WEAK);
  TEN_ASSERT(self, "Invalid argument.");

  self->ctrl_blk = shared_ptr->ctrl_blk;
  ten_atomic_add_fetch(&self->ctrl_blk->weak_cnt, 1);

  return self;
}

ten_weak_ptr_t *ten_weak_ptr_clone(ten_weak_ptr_t *other) {
  TEN_ASSERT(other, "Invalid argument.");
  TEN_ASSERT(ten_weak_ptr_check_integrity(other), "Invalid argument.");

  ten_weak_ptr_t *self =
      ten_smart_ptr_create_without_ctrl_blk(TEN_SMART_PTR_WEAK);
  TEN_ASSERT(self, "Invalid argument.");

  self->ctrl_blk = other->ctrl_blk;
  ten_atomic_add_fetch(&self->ctrl_blk->weak_cnt, 1);

  return self;
}

void ten_weak_ptr_destroy(ten_weak_ptr_t *self) { ten_smart_ptr_destroy(self); }

ten_shared_ptr_t *ten_weak_ptr_lock(ten_weak_ptr_t *self) {
  TEN_ASSERT(self && ten_weak_ptr_check_integrity(self), "Invalid argument.");

  int64_t old_shared_ref_cnt =
      ten_atomic_inc_if_non_zero(&self->ctrl_blk->shared_cnt);
  if (old_shared_ref_cnt == 0) {
    // Failed to lock. The smart_ptr is destroyed.
    return NULL;
  } else if (old_shared_ref_cnt > 0) {
    // Create a new sharedptr.
    ten_shared_ptr_t *shared_ptr =
        ten_smart_ptr_create_without_ctrl_blk(TEN_SMART_PTR_SHARED);

    // The 'shared_cnt' has already been incremented, therefore, we don't need
    // to increment it anymore.
    shared_ptr->ctrl_blk = self->ctrl_blk;
    return shared_ptr;
  } else {
    TEN_ASSERT(0 && "Should not happen.", "Invalid argument.");
    return NULL;
  }
}
