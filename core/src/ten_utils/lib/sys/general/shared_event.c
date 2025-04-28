//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/shared_event.h"

#include <memory.h>
#include <stdlib.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/spinlock.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/lib/waitable_addr.h"
#include "ten_utils/macro/memory.h"

struct ten_shared_event_t {
  ten_waitable_t *signal;
  ten_spinlock_t *lock;
  int auto_reset;
};

ten_shared_event_t *ten_shared_event_create(uint32_t *addr_for_sig,
                                            ten_atomic_t *addr_for_lock,
                                            int init_state, int auto_reset) {
  ten_shared_event_t *ret = NULL;

  if (!addr_for_sig || !addr_for_lock) {
    return NULL;
  }

  ret = (ten_shared_event_t *)TEN_MALLOC(sizeof(*ret));
  if (!ret) {
    return NULL;
  }

  ret->signal = ten_waitable_from_addr(addr_for_sig);
  ret->lock = ten_spinlock_from_addr(addr_for_lock);
  ret->signal->sig = init_state;
  ret->auto_reset = auto_reset;
  return ret;
}

int ten_shared_event_wait(ten_shared_event_t *event, int wait_ms) {
  int64_t timeout_time = 0;
  uint64_t loops = 0;

  if (!event || !event->signal || !event->lock) {
    return -1;
  }

  ten_spinlock_lock(event->lock);
  if (wait_ms == 0) {
    if (!ten_waitable_get(event->signal)) {
      ten_spinlock_unlock(event->lock);
      return -1;
    }

    if (event->auto_reset) {
      ten_waitable_set(event->signal, 0);
    }

    ten_spinlock_unlock(event->lock);
    return 0;
  }

  timeout_time = wait_ms < 0 ? -1 : ten_current_time_ms() + wait_ms;
  while (!ten_waitable_get(event->signal)) {
    int64_t diff = -1;

    if (wait_ms > 0) {
      diff = timeout_time - ten_current_time_ms();

      if (timeout_time > 0 && diff < 0) {
        return -1;
      }
    }

    if (loops++ > 200) {
      continue;
    }

    if (ten_waitable_wait(event->signal, 0, event->lock, (int)diff) != 0) {
      return -1;
    }
  }

  if (event->auto_reset) {
    event->signal = 0;
  }

  ten_spinlock_unlock(event->lock);
  return 0;
}

void ten_shared_event_set(ten_shared_event_t *event) {
  if (!event || !event->signal || !event->lock) {
    return;
  }

  ten_spinlock_lock(event->lock);
  ten_waitable_set(event->signal, 1);
  if (event->auto_reset) {
    ten_waitable_notify(event->signal);
  } else {
    ten_waitable_notify_all(event->signal);
  }
  ten_spinlock_unlock(event->lock);
}

void ten_shared_event_reset(ten_shared_event_t *event) {
  if (!event || !event->signal || !event->lock || event->auto_reset) {
    return;
  }

  ten_spinlock_lock(event->lock);
  ten_waitable_set(event->signal, 0);
  ten_spinlock_unlock(event->lock);
}

void ten_shared_event_destroy(ten_shared_event_t *event) { TEN_FREE(event); }
