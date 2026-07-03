//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/process_mutex.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

#define TEN_PROCESS_MUTEX_CREATE_MODE 0644

typedef struct ten_process_mutex_t {
  sem_t *sem;
  ten_string_t *name;
} ten_process_mutex_t;

ten_process_mutex_t *ten_process_mutex_create(const char *name) {
  TEN_ASSERT(name, "Invalid argument.");

  sem_t *sem = sem_open(name, O_CREAT, TEN_PROCESS_MUTEX_CREATE_MODE, 1);
  if (!sem) {
    TEN_LOGE("Failed to sem_open: %d", errno);
    return NULL;
  }

  ten_process_mutex_t *mutex = TEN_MALLOC(sizeof(ten_process_mutex_t));
  TEN_ASSERT(mutex, "Failed to allocate memory.");

  mutex->sem = sem;
  mutex->name = ten_string_create_formatted("%s", name);

  return mutex;
}

int ten_process_mutex_lock(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  return sem_wait(mutex->sem);
}

int ten_process_mutex_unlock(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  return sem_post(mutex->sem);
}

void ten_process_mutex_destroy(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  int ret = sem_close(mutex->sem);
  if (ret) {
    TEN_LOGE("Failed to sem_close: %d", errno);
  }
  TEN_ASSERT(!ret, "Should not happen.");

  // For the purpose of process mutex, should never call sem_unlink(), because
  // calls to sem_open() to re-create or re-connect to the specified semaphore
  // refer to a new semaphore after sem_unlink() is called. Thus the process
  // mutex mechanism would lose efficacy.
  //
  // Reference from https://www.mkssoftware.com/docs/man3/sem_unlink.3.asp
  //
  // And, never call sem_unlink() will cause file leaks under path /dev/shm
  // before next system restart.
  //
  // TODO(ZhangXianyao): for some use cases of process mutex, such as
  // modifing a file at the same time, recommend using file-lock instead.
  //
  // sem_unlink(ten_string_get_raw_str(mutex->name));

  ten_string_destroy(mutex->name);
  TEN_FREE(mutex);
}
