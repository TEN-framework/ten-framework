//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/mutex.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>

#include "include_internal/ten_utils/lib/mutex.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/thread.h"

typedef struct ten_mutex_t {
  ten_signature_t signature;
  pthread_mutex_t mutex;
  ten_tid_t owner;
} ten_mutex_t;

static bool ten_mutex_check_integrity(ten_mutex_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_MUTEX_SIGNATURE) {
    return false;
  }

  return true;
}

ten_mutex_t *ten_mutex_create(void) {
  ten_mutex_t *mutex = ten_malloc(sizeof(*mutex));
  TEN_ASSERT(mutex, "Failed to allocate memory.");
  if (mutex == NULL) {
    return NULL;
  }

  ten_signature_set(&mutex->signature, TEN_MUTEX_SIGNATURE);

#if defined(_DEBUG)
  // Enable more debugging mechanisms in the debug build.
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif

  pthread_mutex_init(&mutex->mutex,
#if defined(_DEBUG)
                     &attr
#else
                     NULL
#endif
  );

#if defined(_DEBUG)
  pthread_mutexattr_destroy(&attr);
#endif

  return mutex;
}

int ten_mutex_lock(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");
  if (!mutex) {
    return -1;
  }

  TEN_ASSERT(ten_mutex_check_integrity(mutex), "Invalid argument.");

  int rc = pthread_mutex_lock(&mutex->mutex);
  if (rc) {
    TEN_ASSERT(0, "Should not happen: %d", rc);
  }

  mutex->owner = ten_thread_get_id(NULL);
  TEN_ASSERT(mutex->owner, "Should not happen.");

  return rc;
}

int ten_mutex_unlock(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex && ten_mutex_check_integrity(mutex), "Invalid argument.");
  if (!mutex) {
    return -1;
  }

  TEN_ASSERT(mutex->owner, "Should not happen.");
  ten_tid_t prev_owner = mutex->owner;
  mutex->owner = 0;

  int rc = pthread_mutex_unlock(&mutex->mutex);
  TEN_ASSERT(!rc,
             "Should not happen: %d. unlock by: %" PRId64
             ", but hold by: %" PRId64,
             rc, ten_thread_get_id(NULL), prev_owner);

  return rc;
}

void ten_mutex_destroy(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex && ten_mutex_check_integrity(mutex), "Invalid argument.");
  if (!mutex) {
    return;
  }

  pthread_mutex_destroy(&mutex->mutex);
  ten_signature_set(&mutex->signature, 0);
  mutex->owner = 0;

  ten_free(mutex);
}

void *ten_mutex_get_native_handle(ten_mutex_t *mutex) {
  TEN_ASSERT(mutex && ten_mutex_check_integrity(mutex), "Invalid argument.");
  if (!mutex) {
    return NULL;
  }

  return &mutex->mutex;
}

void ten_mutex_set_owner(ten_mutex_t *mutex, ten_tid_t owner) {
  TEN_ASSERT(mutex && owner, "Invalid argument.");
  if (!mutex) {
    return;
  }

  mutex->owner = owner;
}
