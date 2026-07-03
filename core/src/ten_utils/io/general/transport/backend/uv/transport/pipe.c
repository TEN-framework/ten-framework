//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/io/general/transport/backend/uv/stream/pipe.h"

#include <stdlib.h>
#include <uv.h>

#include "include_internal/ten_utils/io/transport.h"
#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/io/stream.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uri.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_transportbackend_pipe_t {
  ten_transportbackend_t base;

  uv_pipe_t *server;
} ten_transportbackend_pipe_t;

static ten_string_t *__get_pipe_name(const ten_string_t *uri) {
  if (uri == NULL) {
    return NULL;
  }

  ten_string_t *host = ten_uri_get_host(ten_string_get_raw_str(uri));
  if (host == NULL) {
    return NULL;
  }

#if defined(_WIN32)

  if (host->buf_size > 3 && host->buf[0] == '\\' && host->buf[1] == '\\' &&
      host->buf[2] == '?' && host->buf[3] == '\\') {
    return host;
  }

  ten_string_t *ret =
      ten_string_create_formatted("\\\\?\\pipe\\%s.sock", host->buf);
  ten_string_destroy(host);
  return ret;

#else

  if (host->buf_size > 0 && host->buf[0] == '/') {
    return host;
  }

  ten_string_t *ret = ten_string_create_formatted("/tmp/%s.sock", host->buf);
  ten_string_destroy(host);
  return ret;

#endif
}

// Destroy all the resources hold by tpbackend object. Call this only when all
// the closing flow is completed.
static void ten_transportbackend_pipe_destroy(
    ten_transportbackend_pipe_t *self) {
  if (!self) {
    return;
  }

  ten_transportbackend_deinit(&self->base);
  TEN_FREE(self);
}

static void ten_transportbackend_pipe_on_close(
    ten_transportbackend_pipe_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_transport_t *transport = self->base.transport;
  TEN_ASSERT(transport, "Invalid argument.");
  ten_transport_on_close(transport);

  ten_transportbackend_pipe_destroy(self);
}

static void on_pipe_server_closed(uv_handle_t *handle) {
  TEN_ASSERT(handle && handle->data, "Invalid argument.");

  ten_transportbackend_pipe_t *self =
      (ten_transportbackend_pipe_t *)handle->data;
  TEN_FREE(handle);

  // Proceed the closing flow.
  ten_transportbackend_pipe_on_close(self);
}

// Trigger the closing flow.
static void ten_transportbackend_pipe_close(ten_transportbackend_t *backend) {
  ten_transportbackend_pipe_t *self = (ten_transportbackend_pipe_t *)backend;

  if (!self) {
    // TEN_LOGD("empty handle, treat as fail");
    return;
  }

  if (ten_atomic_bool_compare_swap(&self->base.is_close, 0, 1)) {
    // TEN_LOGD("Try to close transport (%s)",
    // ten_string_get_raw_str(self->base.name));

    if (self->server) {
      // Close the PIPE server asynchronously.
      uv_close((uv_handle_t *)self->server, on_pipe_server_closed);
    } else {
      // Proceed the closing flow synchronously.
      ten_transportbackend_pipe_on_close(self);
    }
  }
}

static void on_server_connected(uv_connect_t *req, int status) {
  TEN_ASSERT(req, "Invalid argument.");

  ten_stream_t *stream = (ten_stream_t *)req->data;
  TEN_ASSERT(stream && ten_stream_check_integrity(stream), "Invalid argument.");

  TEN_FREE(req);

  ten_transport_t *transport = stream->transport;
  TEN_ASSERT(transport, "Invalid argument.");

  if (status < 0) {
    // TEN_LOGE("Status = %d connect callback, treat as fail", status);
    goto error;
  }

  ten_streambackend_pipe_t *pipe_stream =
      (ten_streambackend_pipe_t *)stream->backend;
  TEN_ASSERT(pipe_stream, "Invalid argument.");

  if (transport && transport->on_server_connected) {
    transport->on_server_connected(transport, stream, status);
  }

  // FALLTHROUGH label
  // The transport is just for connection, not a server-type transport (i.e., a
  // transport with a listening port), so this transport is useless, close it
  // now.

error:
  ten_transport_close(transport);
}

static int ten_transportbackend_pipe_connect(ten_transportbackend_t *backend,
                                             const ten_string_t *dest) {
  ten_string_t *host = NULL;
  ten_stream_t *stream = NULL;

  if (!backend || !dest || ten_string_is_empty(dest)) {
    // TEN_LOGE("Empty handle, treat as fail");
    goto error;
  }

  host = __get_pipe_name(dest);
  if (!host) {
    // TEN_LOGE("Can not get host info from uri %s", dest->buf);
    goto error;
  }

  stream =
      ten_stream_pipe_create_uv(ten_runloop_get_raw(backend->transport->loop));
  TEN_ASSERT(stream, "Invalid argument.");
  stream->transport = backend->transport;

  uv_connect_t *req = (uv_connect_t *)TEN_MALLOC(sizeof(*req));
  if (!req) {
    // TEN_LOGE("Not enough memory");
    goto error;
  }

  memset(req, 0, sizeof(*req));

  ten_streambackend_pipe_t *pipe_stream =
      (ten_streambackend_pipe_t *)stream->backend;
  TEN_ASSERT(pipe_stream, "Invalid argument.");

  req->data = stream;
  uv_pipe_connect(req, pipe_stream->uv_stream, host->buf, on_server_connected);

  ten_string_destroy(host);

  return 0;

error:
  if (host) {
    ten_string_destroy(host);
  }

  if (stream) {
    ten_stream_close(stream);
  }

  return -1;
}

static void on_client_connected(uv_stream_t *server, int status) {
  if (!server || !server->data) {
    // TEN_LOGE("Empty handle, treat as fail");
    goto error;
  }

  if (status < 0) {
    // TEN_LOGE("status = %d, treat as fail", status);
    goto error;
  }

  ten_transportbackend_pipe_t *backend =
      (ten_transportbackend_pipe_t *)server->data;
  ten_transport_t *transport = backend->base.transport;

  ten_stream_t *stream =
      ten_stream_pipe_create_uv(ten_runloop_get_raw(transport->loop));
  stream->transport = transport;

  int rc = uv_accept(
      server,
      (uv_stream_t *)((ten_streambackend_pipe_t *)stream->backend)->uv_stream);
  if (rc != 0) {
    // TEN_LOGE("uv_accept return %d", rc);
    goto error;
  }

  if (transport->on_client_accepted) {
    transport->on_client_accepted(transport, stream, status);
  }

  return;

error:
  if (stream) {
    ten_stream_close(stream);
  }
}

static int ten_transportbackend_pipe_listen(ten_transportbackend_t *backend,
                                            const ten_string_t *dest) {
  ten_transportbackend_pipe_t *self = (ten_transportbackend_pipe_t *)backend;

  if (self->server) {
    // TEN_LOGD("Listen pipe already available");
    return -1;
  }

  uv_pipe_t *server = (uv_pipe_t *)TEN_MALLOC(sizeof(uv_pipe_t));
  TEN_ASSERT(server, "Failed to allocate memory.");
  memset(server, 0, sizeof(uv_pipe_t));

  uv_pipe_init(ten_runloop_get_raw(backend->transport->loop), server, 0);
  server->data = backend;
  self->server = server;

  ten_string_t *host = __get_pipe_name(dest);
  TEN_ASSERT(host, "Failed to get pipe name.");

  int rc = uv_pipe_bind(server, host->buf);
  if (rc != 0) {
    // TEN_LOGD("uv_pipe_bind %s return %d", host->buf, rc);
  }

  rc = uv_listen((uv_stream_t *)server, 128, on_client_connected);
  if (rc != 0) {
    // TEN_LOGD("uv_listen %s return %d", host->buf, rc);
  }

  ten_string_destroy(host);

  return rc;
}

static ten_transportbackend_t *ten_transportbackend_pipe_create(
    ten_transport_t *transport, const ten_string_t *name) {
  ten_transportbackend_pipe_t *self = NULL;

  if (!name || !name->buf || !*name->buf) {
    // TEN_LOGE("Empty handle, treat as fail");
    goto error;
  }

  self = (ten_transportbackend_pipe_t *)TEN_MALLOC(
      sizeof(ten_transportbackend_pipe_t));
  if (!self) {
    // TEN_LOGE("Not enough memory");
    goto error;
  }

  memset(self, 0, sizeof(ten_transportbackend_pipe_t));

  ten_transportbackend_init(&self->base, transport, name);
  self->server = NULL;
  self->base.connect = ten_transportbackend_pipe_connect;
  self->base.listen = ten_transportbackend_pipe_listen;
  self->base.close = ten_transportbackend_pipe_close;

  return (ten_transportbackend_t *)self;

error:
  ten_transportbackend_pipe_destroy(self);
  return NULL;
}

const ten_transportbackend_factory_t uv_tp_backend_pipe = {
    .create = ten_transportbackend_pipe_create,
};
