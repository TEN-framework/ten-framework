//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/event.h"

#include <Windows.h>
#include <stdlib.h>

#include "ten_utils/macro/memory.h"

typedef struct ten_event_t {
  HANDLE event;
} ten_event_t;

ten_event_t *ten_event_create(int init_state, int auto_reset) {
  ten_event_t *event = (ten_event_t *)TEN_MALLOC(sizeof(*event));

  if (!event) {
    return NULL;
  }

  event->event = CreateEvent(NULL, !auto_reset, init_state, NULL);
  return event;
}

int ten_event_wait(ten_event_t *event, int wait_ms) {
  if (!event || !event->event) {
    return -1;
  }

  return (WaitForSingleObject(event->event, wait_ms) == WAIT_OBJECT_0) ? 0 : -1;
}

void ten_event_set(ten_event_t *event) {
  if (!event || !event->event) {
    return;
  }

  SetEvent(event->event);
}

void ten_event_reset(ten_event_t *event) {
  if (!event || !event->event) {
    return;
  }

  ResetEvent(event->event);
}

void ten_event_destroy(ten_event_t *event) {
  if (!event) {
    return;
  }

  if (event->event) {
    CloseHandle(event->event);
  }

  TEN_FREE(event);
}
