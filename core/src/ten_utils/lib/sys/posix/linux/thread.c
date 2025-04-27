//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#include "ten_utils/lib/thread.h"

#include "ten_utils/lib/atomic.h"

#define _GNU_SOURCE
#define __USE_GNU
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

ten_tid_t ten_thread_get_id(ten_thread_t *thread) {
  if (!thread) {
    return syscall(__NR_gettid);
  }

  return ten_atomic_load(&thread->id);
}

int ten_thread_suspend(ten_thread_t *thread) {
  // Not possible in linux
  (void)thread;
  return -1;
}

int ten_thread_resume(ten_thread_t *thread) {
  // Not possible in linux
  (void)thread;
  return -1;
}
