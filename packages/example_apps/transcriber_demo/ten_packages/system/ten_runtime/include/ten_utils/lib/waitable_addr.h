//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef struct ten_spinlock_t ten_spinlock_t;
typedef struct ten_waitable_t {
  uint32_t sig;
} ten_waitable_t;

#define TEN_WAITABLE_INIT {0}

TEN_UTILS_API void ten_waitable_init(ten_waitable_t *wb);

TEN_UTILS_API ten_waitable_t *ten_waitable_from_addr(uint32_t *address);

TEN_UTILS_API int ten_waitable_wait(ten_waitable_t *wb, uint32_t expect,
                                    ten_spinlock_t *lock, int timeout);

TEN_UTILS_API void ten_waitable_notify(ten_waitable_t *wb);

TEN_UTILS_API void ten_waitable_notify_all(ten_waitable_t *wb);

TEN_UTILS_API uint32_t ten_waitable_get(ten_waitable_t *wb);

TEN_UTILS_API void ten_waitable_set(ten_waitable_t *wb, uint32_t val);
