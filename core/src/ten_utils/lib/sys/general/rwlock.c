//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/rwlock.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/lib/rwlock.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

int ten_rwlock_check_integrity(ten_rwlock_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_RWLOCK_SIGNATURE) {
    return 0;
  }
  return 1;
}

static int ten_rwlock_base_init(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");
  return 0;
}

static void ten_rwlock_base_deinit(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");
}

static int ten_pflock_init(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_pflock_t *pflock = (ten_pflock_t *)rwlock;

  pflock->rd.in = 0;
  pflock->rd.out = 0;
  pflock->wr.in = 0;
  pflock->wr.out = 0;

  return 0;
}

static void ten_pflock_deinit(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  // do nothing!
}

static int ten_pflock_lock(ten_rwlock_t *rwlock, int reader) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_pflock_t *pflock = (ten_pflock_t *)rwlock;
  int loops = 0;
  if (reader) {
    uint64_t w = 0;

    // If no writer is present, then the operation has completed
    // successfully.
    w = ten_atomic_fetch_add(&pflock->rd.in, TEN_PFLOCK_RINC) &
        TEN_PFLOCK_WBITS;
    if (w == 0) {
      return 0;
    }

    // Wait for current write phase to complete.
    loops = 0;
    while ((ten_atomic_load(&pflock->rd.in) & TEN_PFLOCK_WBITS) == w) {
      TEN_YIELD(loops);
    }
  } else {
    uint64_t ticket = 0;
    int64_t w = 0;

    // Acquire ownership of write-phase. This is same as ten_tickelock_lock().
    ticket = ten_atomic_fetch_add(&pflock->wr.in, 1);

    loops = 0;
    while (ten_atomic_load(&pflock->wr.out) != ticket) {
      TEN_YIELD(loops);
    }

    // Acquire ticket on read-side in order to allow them to flush. Indicates to
    // any incoming reader that a write-phase is pending.
    //
    // The load of rd.out in wait loop could be executed speculatively.
    w = TEN_PFLOCK_PRES | (ticket & TEN_PFLOCK_PHID);
    ticket = ten_atomic_fetch_add(&pflock->rd.in, w);

    // Wait for any pending readers to flush.
    loops = 0;
    while (ten_atomic_load(&pflock->rd.out) != ticket) {
      TEN_YIELD(loops);
    }
  }

  return 0;
}

static int ten_pflock_unlock(ten_rwlock_t *rwlock, int reader) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_pflock_t *pflock = (ten_pflock_t *)rwlock;

  if (reader) {
    ten_atomic_fetch_add(&pflock->rd.out, TEN_PFLOCK_RINC);
  } else {
    /* Migrate from write phase to read phase. */
    ten_atomic_fetch_and(&pflock->rd.in, TEN_PFLOCK_LSB);

    /* Allow other writers to continue. */
    ten_atomic_fetch_add(&pflock->wr.out, 1);
  }

  return 0;
}

static inline ten_rwlock_t *ten_pflock_create(void) {
  ten_pflock_t *pflock = (ten_pflock_t *)TEN_MALLOC(sizeof(ten_pflock_t));
  TEN_ASSERT(pflock, "Failed to allocate memory.");
  if (pflock == NULL) {
    return NULL;
  }

  memset(pflock, 0, sizeof(ten_pflock_t));

  ten_signature_set(&pflock->base.signature, TEN_RWLOCK_SIGNATURE);
  pflock->base.op.init = ten_pflock_init;
  pflock->base.op.deinit = ten_pflock_deinit;
  pflock->base.op.lock = ten_pflock_lock;
  pflock->base.op.unlock = ten_pflock_unlock;

  return &pflock->base;
}

static inline ten_rwlock_t *ten_native_create(void) {
  ten_native_t *native = (ten_native_t *)TEN_MALLOC(sizeof(ten_native_t));
  TEN_ASSERT(native, "Failed to allocate memory.");
  if (native == NULL) {
    return NULL;
  }

  memset(native, 0, sizeof(ten_native_t));

  ten_signature_set(&native->base.signature, TEN_RWLOCK_SIGNATURE);
  native->base.op.init = ten_native_init;
  native->base.op.deinit = ten_native_deinit;
  native->base.op.lock = ten_native_lock;
  native->base.op.unlock = ten_native_unlock;

  return &native->base;
}

ten_rwlock_t *ten_rwlock_create(TEN_RW_FAIRNESS fair) {
  ten_rwlock_t *rwlock = NULL;
  switch (fair) {
  case TEN_RW_PHASE_FAIR:
    rwlock = ten_pflock_create();
    break;
  case TEN_RW_NATIVE:
    rwlock = ten_native_create();
    break;
  default:
    break;
  }

  if (UNLIKELY(!rwlock)) {
    return NULL;
  }

  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  if (ten_rwlock_base_init(rwlock) != 0) {
    // Prevent memory leak.
    TEN_FREE(rwlock);
    return NULL;
  }

  if (LIKELY(rwlock->op.init != NULL)) {
    if (rwlock->op.init(rwlock) != 0) {
      // Prevent memory leak.
      ten_rwlock_destroy(rwlock);
      return NULL;
    }
  }

  return rwlock;
}

void ten_rwlock_destroy(ten_rwlock_t *lock) {
  if (UNLIKELY(!lock)) {
    return;
  }

  TEN_ASSERT(lock && ten_rwlock_check_integrity(lock), "Invalid argument.");

  if (LIKELY(lock->op.deinit != NULL)) {
    lock->op.deinit(lock);
  }

  ten_rwlock_base_deinit(lock);

  TEN_FREE(lock);
}

int ten_rwlock_lock(ten_rwlock_t *lock, int reader) {
  if (UNLIKELY(!lock)) {
    return -1;
  }

  TEN_ASSERT(lock && ten_rwlock_check_integrity(lock), "Invalid argument.");

  if (LIKELY(lock->op.lock != NULL)) {
    return lock->op.lock(lock, reader);
  }

  return -1;
}

int ten_rwlock_unlock(ten_rwlock_t *lock, int reader) {
  if (UNLIKELY(!lock)) {
    return -1;
  }

  TEN_ASSERT(lock && ten_rwlock_check_integrity(lock), "Invalid argument.");

  if (LIKELY(lock->op.unlock != NULL)) {
    return lock->op.unlock(lock, reader);
  }

  return -1;
}
