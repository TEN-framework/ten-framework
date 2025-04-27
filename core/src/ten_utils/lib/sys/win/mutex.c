//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/mutex.h"

#include <Windows.h>
#include <stdlib.h>

#include "ten_utils/lib/alloc.h"

/**
 * Be careful that mutex from `CreateMutex` is sadly slow
 * Actually the counterpart of pthread_mutex _is_ CRITICAL_SECTION by any means
 */
typedef struct ten_mutex_t {
  CRITICAL_SECTION section;
} ten_mutex_t;

ten_mutex_t *ten_mutex_create(void) {
  ten_mutex_t *mutex = (ten_mutex_t *)TEN_MALLOC(sizeof(*mutex));
  TEN_ASSERT(mutex, "Failed to allocate memory.");
  if (!mutex) {
    return NULL;
  }

  InitializeCriticalSection(&mutex->section);
  return mutex;
}

int ten_mutex_lock(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");
  if (!mutex) {
    return -1;
  }

  EnterCriticalSection(&mutex->section);
  return 0;
}

int ten_mutex_unlock(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");
  if (!mutex) {
    return -1;
  }

  LeaveCriticalSection(&mutex->section);
  return 0;
}

void ten_mutex_destroy(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");
  if (!mutex) {
    return;
  }

  DeleteCriticalSection(&mutex->section);
  TEN_FREE(mutex);
}

void *ten_mutex_get_native_handle(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");
  if (!mutex) {
    return NULL;
  }

  return &mutex->section;
}
