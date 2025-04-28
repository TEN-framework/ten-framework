//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/thread.h"

#include <pthread.h>

#include "ten_utils/macro/check.h"

TEN_UTILS_API ten_thread_t *__get_self();

TEN_UTILS_API void __set_self(ten_thread_t *);

ten_thread_t *ten_thread_self() { return __get_self(); }

void ten_thread_yield() { sched_yield(); }

int ten_thread_equal(ten_thread_t *thread, ten_thread_t *target) {
  pthread_t s = 0;
  pthread_t t = 0;

  if (thread == target) {
    return 1;
  }

  s = thread ? (pthread_t)thread->aux : pthread_self();
  t = target ? (pthread_t)target->aux : pthread_self();

  if (s == 0 || t == 0) {
    return 0;
  }

  return (pthread_equal(s, t) != 0) ? 1 : 0;
}

int ten_thread_equal_to_current_thread(ten_thread_t *thread) {
  TEN_ASSERT(thread, "Invalid argument.");

  pthread_t s = 0;
  pthread_t t = 0;

  s = (pthread_t)thread->aux;
  t = pthread_self();

  if (s == 0 || t == 0) {
    return 0;
  }

  return (pthread_equal(s, t) != 0) ? 1 : 0;
}
