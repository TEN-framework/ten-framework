//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "ten_utils/lib/pid.h"

#include <assert.h>
#include <stdint.h>

#if defined(__linux__)

#include <sys/prctl.h>
#include <sys/types.h>

#if !defined(__ANDROID__)
#include <sys/syscall.h>
#endif

#endif

#if defined(__MACH__)
#include <pthread.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else

#include <unistd.h>

#if defined(__linux__)
#include <linux/limits.h>
#elif defined(__MACH__)
#include <sys/syslimits.h>
#endif

#endif

void ten_get_pid_tid(int64_t *pid, int64_t *tid) {
  // Note: This function will be used in TEN_ASSERT macro, so do not use
  // TEN_ASSERT here.
  assert(pid && tid && "Invalid argument.");

#if defined(OS_WINDOWS)
  *pid = GetCurrentProcessId();
#else
  *pid = getpid();
#endif

#if defined(OS_WINDOWS)
  *tid = GetCurrentThreadId();
#elif defined(OS_LINUX)
  *tid = syscall(SYS_gettid);
#elif defined(OS_MACOS)
  *tid = (int)pthread_mach_thread_np(pthread_self());
#else
#error Platform not supported
#endif
}
