//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/cond.h"

#include <errno.h>  // IWYU pragma: keep
#include <pthread.h>
#include <sys/time.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/memory.h"

struct ten_cond_t {
  pthread_cond_t cond;
};

ten_cond_t *ten_cond_create(void) {
  ten_cond_t *cond = (ten_cond_t *)TEN_MALLOC(sizeof(ten_cond_t));
  if (!cond) {
    TEN_LOGE("Failed to allocate memory.");
    return NULL;
  }

  pthread_cond_init(&cond->cond, NULL);
  return cond;
}

void ten_cond_destroy(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid argument.");
    return;
  }

  pthread_cond_destroy(&cond->cond);
  TEN_FREE(cond);
}

int ten_cond_wait(ten_cond_t *cond, ten_mutex_t *mutex, int64_t wait_ms) {
  struct timeval tv;
  struct timespec ts;
  pthread_mutex_t *lock = (pthread_mutex_t *)ten_mutex_get_native_handle(mutex);
  if (!cond || !lock) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  if (wait_ms < 0) {
    return pthread_cond_wait(&cond->cond, lock);
  } else {
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + wait_ms / 1000;
    ts.tv_nsec = (tv.tv_usec + (wait_ms % 1000) * 1000) * 1000;
    return pthread_cond_timedwait(&cond->cond, lock, &ts);
  }
}

int ten_cond_wait_while(ten_cond_t *cond, ten_mutex_t *mutex,
                        int (*predicate)(void *), void *arg, int64_t wait_ms) {
  int ret = -1;
  struct timespec ts;
  struct timeval tv;
  struct timespec *abs_time = NULL;
  pthread_mutex_t *lock = (pthread_mutex_t *)ten_mutex_get_native_handle(mutex);

  if (!cond || !mutex || !predicate || !lock) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  if (wait_ms == 0) {
    int test_result = predicate(arg);
    return test_result ? -1 : 0;
  }

  if (wait_ms > 0) {
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + wait_ms / 1000;
    ts.tv_nsec = (tv.tv_usec + (wait_ms % 1000) * 1000) * 1000;
    abs_time = &ts;
  }

  while (predicate(arg)) {
    if (abs_time) {
      ret = pthread_cond_timedwait(&cond->cond, lock, abs_time);
    } else {
      ret = pthread_cond_wait(&cond->cond, lock);
    }

    if (ret == ETIMEDOUT || ret == EINVAL) {
      return -ret;
    }
  }

  return 0;
}

int ten_cond_signal(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  return pthread_cond_signal(&cond->cond);
}

int ten_cond_broadcast(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  return pthread_cond_broadcast(&cond->cond);
}
