//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/rwlock.h"

#include <stdint.h>

#include "include_internal/ten_utils/lib/rwlock.h"
#include "ten_utils/macro/check.h"

int ten_native_init(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_native_t *native = (ten_native_t *)rwlock;
  return pthread_rwlock_init(&native->native, NULL);
}

void ten_native_deinit(ten_rwlock_t *rwlock) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_native_t *native = (ten_native_t *)rwlock;
  pthread_rwlock_destroy(&native->native);
}

int ten_native_lock(ten_rwlock_t *rwlock, int reader) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_native_t *native = (ten_native_t *)rwlock;
  return reader ? pthread_rwlock_rdlock(&native->native)
                : pthread_rwlock_wrlock(&native->native);
}

int ten_native_unlock(ten_rwlock_t *rwlock, int reader) {
  TEN_ASSERT(rwlock && ten_rwlock_check_integrity(rwlock), "Invalid argument.");

  ten_native_t *native = (ten_native_t *)rwlock;
  return pthread_rwlock_unlock(&native->native);
}
