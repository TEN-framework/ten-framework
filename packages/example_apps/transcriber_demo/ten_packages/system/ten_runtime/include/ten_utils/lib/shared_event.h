//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/waitable_addr.h"

typedef struct ten_shared_event_t ten_shared_event_t;

TEN_UTILS_API ten_shared_event_t *ten_shared_event_create(
    uint32_t *addr_for_sig, ten_atomic_t *addr_for_lock, int init_state,
    int auto_reset);

TEN_UTILS_API int ten_shared_event_wait(ten_shared_event_t *event, int wait_ms);

TEN_UTILS_API void ten_shared_event_set(ten_shared_event_t *event);

TEN_UTILS_API void ten_shared_event_reset(ten_shared_event_t *event);

TEN_UTILS_API void ten_shared_event_destroy(ten_shared_event_t *event);
