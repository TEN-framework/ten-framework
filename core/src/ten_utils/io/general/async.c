//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/async.h"

#include <stdlib.h>

#include "include_internal/ten_utils/io/runloop.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

static bool ten_async_check_integrity(ten_async_t *self, bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) != TEN_ASYNC_SIGNATURE) {
    return false;
  }
  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }
  return true;
}

static void ten_async_destroy(ten_async_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  ten_string_deinit(&self->name);
  ten_runloop_async_destroy(self->async);
  ten_runloop_async_destroy(self->async_for_close);

  TEN_FREE(self);
}

static void async_cb_entry_point(ten_runloop_async_t *async) {
  TEN_ASSERT(async && ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_async_t *self = (ten_async_t *)(async->data);
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  if (self->on_trigger) {
    self->on_trigger(self, self->on_trigger_data);
  }
}

static void close_cb_entry_point_for_close(ten_runloop_async_t *async) {
  TEN_ASSERT(async && ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_async_t *self = (ten_async_t *)(async->data);
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  if (self->on_closed) {
    self->on_closed(self, self->on_closed_data);
  }

  ten_async_destroy(self);
}

static void close_cb_entry_point(ten_runloop_async_t *async) {
  TEN_ASSERT(async && ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_async_t *self = (ten_async_t *)(async->data);
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  ten_runloop_async_close(self->async_for_close,
                          close_cb_entry_point_for_close);
}

static void async_cb_for_close(ten_runloop_async_t *async) {
  TEN_ASSERT(async && ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_async_t *self = (ten_async_t *)(async->data);
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  ten_runloop_async_close(self->async, close_cb_entry_point);
}

ten_async_t *ten_async_create(const char *name, ten_runloop_t *loop,
                              void *on_trigger, void *on_trigger_data) {
  TEN_ASSERT(name && loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_async_t *self = (ten_async_t *)TEN_MALLOC(sizeof(ten_async_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_ASYNC_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->loop = loop;

  ten_string_init_formatted(&self->name, "%s", name);

  self->on_trigger = on_trigger;
  self->on_trigger_data = on_trigger_data;

  ten_atomic_store(&self->close, 0);
  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->async = ten_runloop_async_create(NULL);
  TEN_ASSERT(self->async, "Invalid argument.");
  self->async->data = self;

  ten_runloop_async_init(self->async, self->loop, async_cb_entry_point);

  self->async_for_close = ten_runloop_async_create(NULL);
  TEN_ASSERT(self->async_for_close, "Invalid argument.");
  self->async_for_close->data = self;

  ten_runloop_async_init(self->async_for_close, self->loop, async_cb_for_close);

  return self;
}

void ten_async_trigger(ten_async_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in any threads.
      ten_async_check_integrity(self, false), "Invalid argument.");
  ten_runloop_async_notify(self->async);
}

void ten_async_close(ten_async_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  if (ten_atomic_bool_compare_swap(&self->close, 0, 1)) {
    ten_runloop_async_notify(self->async_for_close);
  }
}

void ten_async_set_on_closed(ten_async_t *self, void *on_closed,
                             void *on_closed_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_async_check_integrity(self, true), "Invalid argument.");

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}
