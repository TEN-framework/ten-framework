//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/cond.h"

#include <Windows.h>
#include <stdlib.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

struct ten_cond_t {
  CONDITION_VARIABLE cond;
};

ten_cond_t *ten_cond_create(void) {
  ten_cond_t *cond = (ten_cond_t *)TEN_MALLOC(sizeof(ten_cond_t));
  TEN_ASSERT(cond, "Failed to allocate memory.");
  if (cond == NULL) {
    return NULL;
  }

  InitializeConditionVariable(&cond->cond);
  return cond;
}

void ten_cond_destroy(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid_argument.");
    return;
  }

  TEN_FREE(cond);
}

int ten_cond_wait(ten_cond_t *cond, ten_mutex_t *mutex, int64_t wait_ms) {
  CRITICAL_SECTION *lock =
      (CRITICAL_SECTION *)ten_mutex_get_native_handle(mutex);

  if (!cond || !lock) {
    return -1;
  }

  if (wait_ms < 0) {
    return SleepConditionVariableCS(&cond->cond, lock, INFINITE) ? 0 : -1;
  }

  DWORD timeout_ms = (wait_ms > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)wait_ms;
  return SleepConditionVariableCS(&cond->cond, lock, timeout_ms) ? 0 : -1;
}

int ten_cond_wait_while(ten_cond_t *cond, ten_mutex_t *mutex,
                        int (*predicate)(void *), void *arg, int64_t wait_ms) {
  BOOL ret = FALSE;
  CRITICAL_SECTION *lock =
      (CRITICAL_SECTION *)ten_mutex_get_native_handle(mutex);

  if (!cond || !mutex || !predicate || !lock) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  if (wait_ms == 0) {
    int test_result = predicate(arg);
    return test_result ? -1 : 0;
  }

  BOOL wait_forever = wait_ms < 0;

  while (predicate(arg)) {
    if (wait_forever) {
      ret = SleepConditionVariableCS(&cond->cond, lock, INFINITE);
    } else {
      DWORD timeout_ms = (wait_ms > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)wait_ms;
      int64_t begin = ten_current_time_ms();
      ret = SleepConditionVariableCS(&cond->cond, lock, timeout_ms);
      wait_ms -= (ten_current_time_ms() - begin);

      if (wait_ms <= 0 && predicate(arg)) {
        // Timeout but condition is not satisfied.
        return -1;
      }
    }

    if (!ret) {
      DWORD error = GetLastError();
      TEN_LOGE("SleepConditionVariableCS failed with error: %lu", error);
      return -1;
    }
  }

  return 0;
}

int ten_cond_signal(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  WakeConditionVariable(&cond->cond);
  return 0;
}

int ten_cond_broadcast(ten_cond_t *cond) {
  if (!cond) {
    TEN_LOGE("Invalid_argument.");
    return -1;
  }

  WakeAllConditionVariable(&cond->cond);
  return 0;
}
