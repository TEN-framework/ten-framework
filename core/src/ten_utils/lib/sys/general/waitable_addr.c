//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/waitable_addr.h"

#include <memory.h>

#include "ten_utils/lib/atomic.h"

uint32_t ten_waitable_get(ten_waitable_t *wb) {
  uint32_t ret = 0;

  ten_memory_barrier();
  ret = wb->sig;
  ten_memory_barrier();
  return ret;
}

void ten_waitable_set(ten_waitable_t *wb, uint32_t val) {
  ten_memory_barrier();
  wb->sig = val;
  ten_memory_barrier();
}
