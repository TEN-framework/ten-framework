//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/transport.h"

#include <stdlib.h>

#include "include_internal/ten_utils/io/runloop.h"
#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/io/general/transport/backend/factory.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

// Destroy all the resources hold by this transport object.
static void ten_transport_destroy(ten_transport_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->lock) {
    ten_mutex_destroy(self->lock);
  }
  TEN_FREE(self);
}

ten_transport_t *ten_transport_create(ten_runloop_t *loop) {
  ten_transport_t *self = NULL;

  if (!loop) {
    goto error;
  }

  self = (ten_transport_t *)TEN_MALLOC(sizeof(*self));
  if (!self) {
    goto error;
  }

  memset(self, 0, sizeof(*self));

  ten_atomic_store(&self->close, 0);

  self->lock = ten_mutex_create();
  if (!self->lock) {
    goto error;
  }

  self->loop = loop;
  self->user_data = NULL;
  self->backend = NULL;
  self->on_server_connected = NULL;
  self->on_server_connected_user_data = NULL;
  self->on_client_accepted = NULL;
  self->on_client_accepted_user_data = NULL;
  self->on_closed = NULL;
  self->on_closed_user_data = NULL;
  self->drop_type = TEN_TRANSPORT_DROP_NEW;
  self->drop_when_full = 1;

  return self;

error:
  if (self) {
    ten_transport_destroy(self);
  }
  return NULL;
}

void ten_transport_set_close_cb(ten_transport_t *self, void *close_cb,
                                void *close_cb_data) {
  TEN_ASSERT(self, "Invalid argument.");

  self->on_closed = close_cb;
  self->on_closed_user_data = close_cb_data;
}

// The actual closing flow.
void ten_transport_on_close(ten_transport_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  // The final step in the closing flow is to notify the outer environment that
  // we are closed.
  if (self->on_closed) {
    self->on_closed(self->on_closed_user_data);
  }

  ten_transport_destroy(self);
}

int ten_transport_close(ten_transport_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_atomic_bool_compare_swap(&self->close, 0, 1)) {
    // Trigger the closing flow of the backend, or proceed the closing flow
    // directly.
    if (self->backend) {
      self->backend->close(self->backend);
    } else {
      ten_transport_on_close(self);
    }

    return 0;
  }

  return -1;
}

enum TEN_TRANSPORT_DROP_TYPE ten_transport_get_drop_type(
    ten_transport_t *self) {
  TEN_TRANSPORT_DROP_TYPE ret = TEN_TRANSPORT_DROP_NEW;

  if (!self) {
    return ret;
  }

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  ret = self->drop_type;

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  return ret;
}

void ten_transport_set_drop_type(ten_transport_t *self,
                                 TEN_TRANSPORT_DROP_TYPE drop_type) {
  if (!self) {
    return;
  }

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  self->drop_type = drop_type;

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");
}

int ten_transport_drop_required(ten_transport_t *self) {
  int ret = 0;

  if (!self) {
    return ret;
  }

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  ret = self->drop_when_full;

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  return ret;
}

void ten_transport_set_drop_when_full(ten_transport_t *self, int drop) {
  if (!self) {
    return;
  }

  TEN_UNUSED int rc = ten_mutex_lock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");

  self->drop_when_full = drop;

  rc = ten_mutex_unlock(self->lock);
  TEN_ASSERT(!rc, "Invalid argument.");
}

int ten_transport_listen(ten_transport_t *self, const ten_string_t *my_uri) {
  const ten_transportbackend_factory_t *factory = NULL;

  if (!self || self->backend) {
    // TEN_LOGE("Empty transport");
    return -1;
  }

  if (!my_uri || ten_string_is_empty(my_uri)) {
    // TEN_LOGD("Empty uri");
    return -1;
  }

  factory = ten_get_transportbackend_factory(self->loop->impl, my_uri);
  if (!factory) {
    // TEN_LOGE("No valid backend for uri %s", my_uri->buf);
    return -1;
  }

  self->backend = factory->create(self, my_uri);
  if (!self->backend) {
    // TEN_LOGE("No valid backend for uri %s", my_uri->buf);
    return -1;
  }

  return self->backend->listen(self->backend, my_uri);
}

int ten_transport_connect(ten_transport_t *self, ten_string_t *dest) {
  TEN_ASSERT(self && dest, "Invalid argument.");

  if (!self || !dest) {
    return -1;
  }

  const ten_transportbackend_factory_t *factory =
      ten_get_transportbackend_factory(self->loop->impl, dest);
  if (!factory) {
    return -1;
  }

  ten_transportbackend_t *backend = factory->create(self, dest);
  if (!backend) {
    return -1;
  }

  self->backend = backend;
  return backend->connect(backend, dest);
}
