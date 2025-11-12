//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/spinlock.h"

#include <memory.h>

#include "ten_utils/lib/atomic.h"

void ten_spinlock_init(ten_spinlock_t *spin) {
  static const ten_spinlock_t initializer = TEN_SPINLOCK_INIT;
  *spin = initializer;
}

ten_spinlock_t *ten_spinlock_from_addr(ten_atomic_t *addr) {
  ten_spinlock_t *spin = (ten_spinlock_t *)addr;
  ten_spinlock_init(spin);
  return spin;
}

void ten_spinlock_lock(ten_spinlock_t *spin) {
  uint64_t loops = 0;
  while (ten_atomic_test_set(&spin->lock, 1) == 1) {
    if (loops++ > 200) {
      ten_thread_pause_cpu();
    }
  }
}

int ten_spinlock_trylock(ten_spinlock_t *spin) {
  return (ten_atomic_test_set(&spin->lock, 1) != 1) ? 1 : 0;
}

void ten_spinlock_unlock(ten_spinlock_t *spin) {
  ten_memory_barrier();
  spin->lock = 0;
  ten_memory_barrier();
}

void ten_recursive_spinlock_init(ten_recursive_spinlock_t *spin) {
  static const ten_recursive_spinlock_t initializer =
      TEN_RECURSIVE_SPINLOCK_INIT;
  *spin = initializer;
}

ten_recursive_spinlock_t *ten_recursive_spinlock_from_addr(uint8_t *addr) {
  ten_recursive_spinlock_t *spin = (ten_recursive_spinlock_t *)addr;
  ten_recursive_spinlock_init(spin);
  return spin;
}

void ten_recursive_spinlock_lock(ten_recursive_spinlock_t *spin) {
  ten_tid_t tid = ten_thread_get_id(NULL);
  ten_pid_t pid = ten_task_get_id();

  if (spin->tid != tid || spin->pid != pid) {
    ten_spinlock_lock(&spin->lock);
    spin->pid = pid;
    spin->tid = tid;
  }

  spin->count++;
}

int ten_recursive_spinlock_trylock(ten_recursive_spinlock_t *spin) {
  ten_tid_t tid = ten_thread_get_id(NULL);
  ten_pid_t pid = ten_task_get_id();

  if (spin->tid != tid || spin->pid != pid) {
    if (ten_spinlock_trylock(&spin->lock) == 0) {
      return 0;
    }

    spin->tid = tid;
    spin->pid = pid;
  }

  spin->count++;
  return 1;
}

void ten_recursive_spinlock_unlock(ten_recursive_spinlock_t *spin) {
  if (--(spin->count) == 0) {
    spin->tid = -1;
    spin->pid = -1;
    ten_spinlock_unlock(&spin->lock);
  }
}
