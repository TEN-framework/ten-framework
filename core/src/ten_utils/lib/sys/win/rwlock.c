//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/rwlock.h"

#include <stdint.h>

// clang-format off
// Stupid Windows doesn't handle header files well
// Include order matters in Windows
#include <windows.h>
#include <synchapi.h>
// clang-format on

#include "include_internal/ten_utils/lib/rwlock.h"

int ten_native_init(ten_rwlock_t *rwlock) {
  ten_native_t *native = (ten_native_t *)rwlock;
  InitializeSRWLock(&native->native);
  return 0;
}

void ten_native_deinit(ten_rwlock_t *rwlock) {}

int ten_native_lock(ten_rwlock_t *rwlock, int reader) {
  ten_native_t *native = (ten_native_t *)rwlock;
  if (reader) {
    AcquireSRWLockShared(&native->native);
  } else {
    AcquireSRWLockExclusive(&native->native);
  }
  return 0;
}

int ten_native_unlock(ten_rwlock_t *rwlock, int reader) {
  ten_native_t *native = (ten_native_t *)rwlock;
  if (reader) {
    ReleaseSRWLockShared(&native->native);
  } else {
    ReleaseSRWLockExclusive(&native->native);
  }
  return 0;
}
