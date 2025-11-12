//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/process_mutex.h"

#include <Windows.h>
#include <stdlib.h>

#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

#define TEN_PROCESS_MUTEX_CREATE_MODE 0644

typedef struct ten_process_mutex_t {
  void *handle;
  ten_string_t *name;
} ten_process_mutex_t;

ten_process_mutex_t *ten_process_mutex_create(const char *name) {
  TEN_ASSERT(name, "Invalid argument.");

  ten_process_mutex_t *mutex = TEN_MALLOC(sizeof(ten_process_mutex_t));
  TEN_ASSERT(mutex, "Failed to allocate memory.");

  mutex->handle = NULL;
  mutex->handle = CreateMutex(NULL, false, name);
  TEN_ASSERT(mutex->handle, "Failed to create mutex.");
  mutex->name = ten_string_create_formatted("%s", name);

  return mutex;
}

int ten_process_mutex_lock(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  DWORD ret = WaitForSingleObject(mutex->handle, INFINITE);
  if (ret != WAIT_OBJECT_0) {
    return -1;
  }

  return 0;
}

int ten_process_mutex_unlock(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  if (ReleaseMutex(mutex->handle)) {
    return 0;
  }

  return -1;
}

void ten_process_mutex_destroy(ten_process_mutex_t *mutex) {
  TEN_ASSERT(mutex, "Invalid argument.");

  CloseHandle(mutex->handle);
  ten_string_destroy(mutex->name);
  TEN_FREE(mutex);
}
