//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/reflock.h"

#include <memory.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void __reflock_signal_event(ten_reflock_t *lock) {
  ten_event_set(lock->event);
}

static void __reflock_await_event(ten_reflock_t *lock) {
  ten_event_wait(lock->event, -1);
}

void ten_reflock_init(ten_reflock_t *reflock) {
  reflock->state = 0;
  reflock->event = ten_event_create(0, 1);
}

void ten_reflock_ref(ten_reflock_t *reflock) {
  int64_t state = ten_atomic_add_fetch(&reflock->state, TEN_REFLOCK_REF);

  /* Verify that the counter didn't overflow and the lock isn't destroyed. */
  TEN_ASSERT((state & TEN_REFLOCK_DESTROY_MASK) == 0, "Invalid state.");
  (void)(state);
}

void ten_reflock_unref(ten_reflock_t *reflock) {
  int64_t state = ten_atomic_sub_fetch(&reflock->state, TEN_REFLOCK_REF);

  /* Verify that the lock was referenced and not already destroyed. */
  TEN_ASSERT((state & TEN_REFLOCK_DESTROY_MASK & ~TEN_REFLOCK_DESTROY) == 0,
             "Invalid state.");
  if (UNLIKELY((state & TEN_REFLOCK_DESTROY_MASK & ~TEN_REFLOCK_DESTROY) !=
               0)) {
    return;
  }

  if (state == TEN_REFLOCK_DESTROY) {
    __reflock_signal_event(reflock);
  }
}

void ten_reflock_unref_destroy(ten_reflock_t *reflock) {
  int64_t state = ten_atomic_add_fetch(&reflock->state,
                                       TEN_REFLOCK_DESTROY - TEN_REFLOCK_REF);
  int64_t ref_count = state & TEN_REFLOCK_REF_MASK;

  /* Verify that the lock was referenced and not already destroyed. */
  TEN_ASSERT((state & TEN_REFLOCK_DESTROY_MASK) == TEN_REFLOCK_DESTROY,
             "Invalid state.");
  if (UNLIKELY((state & TEN_REFLOCK_DESTROY_MASK) != TEN_REFLOCK_DESTROY)) {
    return;
  }

  if (ref_count != 0) {
    __reflock_await_event(reflock);
  }

  state = ten_atomic_test_set(&reflock->state, TEN_REFLOCK_POISON);
  TEN_ASSERT(state == TEN_REFLOCK_DESTROY, "Invalid state.");

  ten_event_destroy(reflock->event);
  reflock->event = NULL;
}
