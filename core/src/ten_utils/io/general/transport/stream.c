//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/stream.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_stream_check_integrity(ten_stream_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_STREAM_SIGNATURE) {
    return false;
  }
  return true;
}

void ten_stream_init(ten_stream_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, TEN_STREAM_SIGNATURE);
  ten_atomic_store(&self->close, 0);
  self->on_message_free = NULL;
  self->on_message_read = NULL;
  self->on_message_sent = NULL;
  self->on_closed = NULL;
  self->on_closed_data = NULL;
}

int ten_stream_send(ten_stream_t *self, const char *msg, uint32_t size,
                    void *user_data) {
  TEN_ASSERT(self && ten_stream_check_integrity(self) && msg && (size > 0) &&
                 self->backend,
             "Invalid argument.");

  return self->backend->write(self->backend, msg, size, user_data);
}

int ten_stream_start_read(ten_stream_t *self) {
  TEN_ASSERT(self && ten_stream_check_integrity(self) && self->backend,
             "Invalid argument.");

  return self->backend->start_read(self->backend);
}

int ten_stream_stop_read(ten_stream_t *self) {
  TEN_ASSERT(self && ten_stream_check_integrity(self) && self->backend,
             "Invalid argument.");

  return self->backend->stop_read(self->backend);
}

static void ten_stream_destroy(ten_stream_t *self) {
  TEN_ASSERT(self && ten_stream_check_integrity(self), "Invalid argument.");

  TEN_FREE(self);
}

void ten_stream_on_close(ten_stream_t *self) {
  TEN_ASSERT(self && ten_stream_check_integrity(self), "Invalid argument.");

  if (self->on_closed) {
    self->on_closed(self->on_closed_data);
  }
  ten_stream_destroy(self);
}

void ten_stream_close(ten_stream_t *self) {
  TEN_ASSERT(self && ten_stream_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->backend, "Invalid argument.");

  if (ten_atomic_bool_compare_swap(&self->close, 0, 1)) {
    self->backend->close(self->backend);
  }
}

void ten_stream_set_on_closed(ten_stream_t *self, void *on_closed,
                              void *on_closed_data) {
  TEN_ASSERT(self && ten_stream_check_integrity(self), "Invalid argument.");

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}
