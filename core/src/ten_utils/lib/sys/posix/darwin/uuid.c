//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from https://github.com/gpakosz/uuid4/
//
#include "ten_utils/lib/uuid.h"

#include <mach/mach_time.h>
#include <pthread.h>
#include <unistd.h>

void ten_uuid4_seed(ten_uuid4_state_t *seed) {
  static uint64_t state0 = 0;

  uint64_t time = mach_absolute_time();

  *seed = state0++ + time;

  uint32_t pid = (uint32_t)getpid();
  uint64_t tid = 0;
  pthread_threadid_np(NULL, &tid);
  *seed = *seed * 6364136223846793005U +
          ((uint64_t)(ten_uuid4_mix(ten_uuid4_hash(pid),
                                    ten_uuid4_hash((uint32_t)tid)))
           << 32);
  *seed = *seed * 6364136223846793005U + (uintptr_t)getpid;
  *seed = *seed * 6364136223846793005U + (uintptr_t)ten_uuid4_gen_from_seed;
}
