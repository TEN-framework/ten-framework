//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/thread.h"

#include <Windows.h>
#include <stdlib.h>

#include "ten_utils/lib/thread_local.h"
#include "ten_utils/macro/check.h"

TEN_UTILS_API ten_thread_t *__get_self();

TEN_UTILS_API void __set_self(ten_thread_t *);

int ten_thread_suspend(ten_thread_t *thread) {
  HANDLE t;

  if (thread && !thread->aux) {
    return -1;
  }

  if (thread) {
    t = (HANDLE)thread->aux;
  } else {
    t = GetCurrentThread();
  }

  if (SuspendThread(t) == (DWORD)-1) {
    return -1;
  }

  return 0;
}

int ten_thread_resume(ten_thread_t *thread) {
  HANDLE t;

  if (thread && !thread->aux) {
    return -1;
  }

  if (thread) {
    t = (HANDLE)thread->aux;
  } else {
    t = GetCurrentThread();
  }

  if (ResumeThread(t) == (DWORD)-1) {
    return -1;
  }

  return 0;
}

ten_tid_t ten_thread_get_id(ten_thread_t *thread) {
  if (!thread) {
    return GetCurrentThreadId();
  }

  return ten_atomic_load(&thread->id);
}

ten_thread_t *ten_thread_self() { return __get_self(); }

void ten_thread_yield() { SwitchToThread(); }

static const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

int ten_thread_set_name(ten_thread_t *thread, const char *name) {
  ten_atomic_t tid = 0;
  THREADNAME_INFO info;

  if (!name || !*name) {
    return -1;
  }

  tid = ten_thread_get_id(thread);

  // Welcom to the wild world
  // This is how Microsoft handle "set thread name" thing [1]. Good for him.
  //
  // [1]:
  // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2019
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = (DWORD)tid;
  info.dwFlags = 0;

#pragma warning(push)
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD),
                   (DWORD_PTR *)(&info));
  } __except (EXCEPTION_CONTINUE_EXECUTION) {
  }
#pragma warning(pop)

  return 0;
}

int ten_thread_equal(ten_thread_t *thread, ten_thread_t *target) {
  DWORD s = 0;
  DWORD t = 0;

  if (thread == target) {
    return 1;
  }

  s = (thread && thread->aux) ? GetThreadId((HANDLE)thread->aux)
                              : GetCurrentThreadId();
  t = (target && target->aux) ? GetThreadId((HANDLE)target->aux)
                              : GetCurrentThreadId();

  if (s == 0 || t == 0) {
    return 0;
  }

  return s == t;
}

int ten_thread_equal_to_current_thread(ten_thread_t *thread) {
  TEN_ASSERT(thread && thread->aux, "Invalid argument.");

  DWORD s = 0;
  DWORD t = 0;

  s = GetThreadId((HANDLE)thread->aux);
  t = GetCurrentThreadId();

  if (s == 0 || t == 0) {
    return 0;
  }

  return s == t;
}
