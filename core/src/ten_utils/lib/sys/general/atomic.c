//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/atomic.h"

#include <stdint.h>

// NOTE: This file will be used in the TEN backtrace module, so do _not_ use
// TEN_ASSERT or any other mechanisms which might involve backtrace dump.

int64_t ten_atomic_load(volatile ten_atomic_t *a) {
  int64_t ret = 0;
  ten_memory_barrier();
  ret = *(a);
  ten_memory_barrier();
  return ret;
}

void ten_atomic_store(volatile ten_atomic_t *a, int64_t v) {
  ten_memory_barrier();
  *a = v;
  ten_memory_barrier();
}
