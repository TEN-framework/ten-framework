//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#include "ten_utils/lib/thread.h"

#include <string.h>

#include "ten_utils/lib/atomic.h"

#define _GNU_SOURCE
#define __USE_GNU
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

int ten_thread_set_name(ten_thread_t *thread, const char *name) {
  pthread_t t = 0;

  if (!name || !*name) {
    return -1;
  }

  if (!thread) {
    t = pthread_self();
  } else {
    t = (pthread_t)thread->aux;
  }

  // Linux and Android require names (with nul) fit in 16 chars, otherwise
  // pthread_setname_np() returns ERANGE (34).
  char thread_name[16];
  strncpy(thread_name, name, sizeof(thread_name) - 1);
  thread_name[sizeof(thread_name) - 1] = '\0';

  return pthread_setname_np(t, thread_name);
}

void ten_thread_set_affinity(ten_thread_t *thread, uint64_t mask) {
  pthread_t t;
  cpu_set_t cpuset;

  if (!thread) {
    t = pthread_self();
  } else {
    t = (pthread_t)thread->aux;
  }

  CPU_ZERO(&cpuset);
  for (int i = 0; i < 64; i++) {
    if (mask & (1ULL << i)) {
      CPU_SET(i, &cpuset);
    }
  }

  pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
}
