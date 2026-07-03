//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/event.h"

#include <sys/time.h>

#include "include_internal/ten_utils/lib/mutex.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/cond.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"

#define TEN_EVENT_SIGNATURE 0xB5F7D324A07B41E4U

typedef struct ten_event_t {
  ten_signature_t signature;

  ten_mutex_t *mutex;
  ten_cond_t *cond;
  int signal;
  int auto_reset;
} ten_event_t;

static int ten_event_check_integrity(ten_event_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_EVENT_SIGNATURE) {
    return 0;
  }
  return 1;
}

static void ten_event_init(ten_event_t *event, int init_state, int auto_reset) {
  TEN_ASSERT(event, "Invalid argument.");

  ten_signature_set(&event->signature, TEN_EVENT_SIGNATURE);

  event->mutex = ten_mutex_create();
  event->cond = ten_cond_create();
  event->signal = init_state;
  event->auto_reset = auto_reset;
}

ten_event_t *ten_event_create(int init_state, int auto_reset) {
  ten_event_t *event = TEN_MALLOC(sizeof(*event));
  TEN_ASSERT(event, "Failed to allocate memory.");

  ten_event_init(event, init_state, auto_reset);

  return event;
}

static int ten_event_no_signal(void *arg) {
  ten_event_t *event = (ten_event_t *)arg;
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  return !event->signal;
}

int ten_event_wait(ten_event_t *event, int wait_ms) {
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  int ret = -1;
  struct timespec ts;
  struct timeval tv;
  struct timespec *abs_time = NULL;

  ten_mutex_lock(event->mutex);

  ret = ten_cond_wait_while(event->cond, event->mutex, ten_event_no_signal,
                            event, wait_ms);

  ten_mutex_set_owner(event->mutex, ten_thread_get_id(NULL));

  if (event->auto_reset) {
    event->signal = 0;
  }

  ten_mutex_unlock(event->mutex);

  return ret;
}

void ten_event_set(ten_event_t *event) {
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  ten_mutex_lock(event->mutex);
  event->signal = 1;
  if (event->auto_reset) {
    ten_cond_signal(event->cond);
  } else {
    ten_cond_broadcast(event->cond);
  }
  ten_mutex_unlock(event->mutex);
}

void ten_event_reset(ten_event_t *event) {
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  if (event->auto_reset) {
    return;
  }

  ten_mutex_lock(event->mutex);
  event->signal = 0;
  ten_mutex_unlock(event->mutex);
}

static void ten_event_deinit(ten_event_t *event) {
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  ten_mutex_destroy(event->mutex);
  ten_cond_destroy(event->cond);
}

void ten_event_destroy(ten_event_t *event) {
  TEN_ASSERT(event && ten_event_check_integrity(event), "Invalid argument.");

  ten_event_deinit(event);
  TEN_FREE(event);
}
