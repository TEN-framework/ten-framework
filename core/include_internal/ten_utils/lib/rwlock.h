//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#if !defined(_WIN32)
#include <pthread.h>
#else
// clang-format off
// Stupid Windows doesn't handle header files well
// Include order matters in Windows
#include <windows.h>
#include <synchapi.h>
// clang-format on
#endif

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/rwlock.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/thread.h"  // IWYU pragma: keep
#include "ten_utils/lib/time.h"    // IWYU pragma: keep

#define TEN_RWLOCK_SIGNATURE 0xF033C89F0985EB79U

// Uses a spin/wait loop that calls ten_thread_pause_cpu(), then
// ten_thread_yield(), then sleeps 10 ms if the loop is still not satisfied.
// This avoids busy waiting on a single CPU core. It is a typical pattern.
#define TEN_YIELD(loop)       \
  do {                        \
    loop++;                   \
    if (loop < 100) {         \
      ten_thread_pause_cpu(); \
    } else if (loop < 1000) { \
      ten_thread_yield();     \
    } else {                  \
      ten_sleep_ms(10);       \
    }                         \
  } while (0)

/*
 * Allocation of bits to reader
 *
 * 64                 4 3 2 1 0
 * +-------------------+---+-+-+
 * | rin: reads issued |x|x| | |
 * +-------------------+---+-+-+
 *                          ^ ^
 *                          | |
 * PRES: writer present ----/ |
 * PHID: writer phase id -----/
 *
 * 64                4 3 2 1 0
 * +------------------+------+
 * |rout:read complete|unused|
 * +------------------+------+
 *
 * The maximum number of readers is 2^60 - 1 (more then enough)
 */

// A few bits in `rd.in` for indicating "writer present" (TEN_PFLOCK_PRES) and
// the "phase ID" (TEN_PFLOCK_PHID)
//
// Constants used to map the bits in reader counter.
#define TEN_PFLOCK_WBITS 0x3               // Writer bits in reader.
#define TEN_PFLOCK_PRES 0x2                // Writer present bit.
#define TEN_PFLOCK_PHID 0x1                // Phase ID bit.
#define TEN_PFLOCK_LSB 0xFFFFFFFFFFFFFFF0  // reader bits.
#define TEN_PFLOCK_RINC 0x10               // Reader increment.

typedef struct ten_rwlock_op_t {
  int (*init)(ten_rwlock_t *rwlock);
  void (*deinit)(ten_rwlock_t *rwlock);
  int (*lock)(ten_rwlock_t *rwlock, int reader);
  int (*unlock)(ten_rwlock_t *rwlock, int reader);
} ten_rwlock_op_t;

struct ten_rwlock_t {
  ten_signature_t signature;
  ten_rwlock_op_t op;
};

typedef struct ten_pflock_t {
  ten_rwlock_t base;

  // rd.in / rd.out for tracking how many readers have begun and completed.
  // wr.in / wr.out for a ticket mechanism on the writer side.
  struct {
    ten_atomic_t in;
    ten_atomic_t out;
  } rd, wr;
} ten_pflock_t;

typedef struct ten_native_t {
  ten_rwlock_t base;

#if defined(_WIN32)
  SRWLOCK native;
#else
  pthread_rwlock_t native;
#endif

} ten_native_t;

TEN_UTILS_PRIVATE_API int ten_rwlock_check_integrity(ten_rwlock_t *self);

TEN_UTILS_PRIVATE_API int ten_native_init(ten_rwlock_t *rwlock);

TEN_UTILS_PRIVATE_API void ten_native_deinit(ten_rwlock_t *rwlock);

TEN_UTILS_PRIVATE_API int ten_native_lock(ten_rwlock_t *rwlock, int reader);

TEN_UTILS_PRIVATE_API int ten_native_unlock(ten_rwlock_t *rwlock, int reader);
