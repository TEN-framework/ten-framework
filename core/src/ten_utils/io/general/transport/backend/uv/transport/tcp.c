//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/io/general/transport/backend/uv/stream/tcp.h"

#include <stdlib.h>
#include <uv.h>

#include "include_internal/ten_utils/io/transport.h"
#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/io/stream.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uri.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/mark.h"

typedef struct ten_transportbackend_tcp_t {
  ten_transportbackend_t base;

  uv_stream_t *server;
} ten_transportbackend_tcp_t;

// Destroy all the resources hold by tpbackend object. Call this only when
// all the closing flow is completed.
static void ten_transportbackend_tcp_destroy(ten_transportbackend_tcp_t *self) {
  if (!self) {
    return;
  }

  ten_transportbackend_deinit(&self->base);
  TEN_FREE(self);
}

static void ten_transportbackend_tcp_on_close(
    ten_transportbackend_tcp_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_transport_t *transport = self->base.transport;
  TEN_ASSERT(transport, "Invalid argument.");

  ten_transport_on_close(transport);

  ten_transportbackend_tcp_destroy(self);
}

static void on_tcp_server_closed(uv_handle_t *handle) {
  TEN_ASSERT(handle && handle->data, "Invalid argument.");

  ten_transportbackend_tcp_t *self = (ten_transportbackend_tcp_t *)handle->data;
  TEN_FREE(handle);

  // Proceed the closing flow.
  ten_transportbackend_tcp_on_close(self);
}

// Trigger the closing flow.
static void ten_transportbackend_tcp_close(ten_transportbackend_t *backend) {
  ten_transportbackend_tcp_t *self = (ten_transportbackend_tcp_t *)backend;

  if (!self) {
    // TEN_LOGE("Empty handle, treat as fail");
    return;
  }

  if (ten_atomic_bool_compare_swap(&self->base.is_close, 0, 1)) {
    // TEN_LOGD("Try to close transport (%s)",
    // ten_string_get_raw_str(self->base.name));

    if (self->server) {
      // Close the TCP server asynchronously.
      uv_close((uv_handle_t *)self->server, on_tcp_server_closed);
    } else {
      // Proceed the closing flow synchronously.
      ten_transportbackend_tcp_on_close(self);
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

  // No matter success or failed, trigger the callback the notify the status to
  // the original requester. Because the original requester would do some
  // cleanup when the connection failed.
  if (transport && transport->on_server_connected) {
    transport->on_server_connected(transport, stream, status);
  }

  // The transport is just for connection, not a server-type transport (i.e., a
  // transport with a listening port), so this transport is useless, close it
  // now.
  ten_transport_close(transport);

  if (status < 0) {
    // TEN_LOGE("Status = %d connect callback, treat as fail", status);
  } else {
    ten_streambackend_tcp_t *tcp_stream =
        (ten_streambackend_tcp_t *)stream->backend;
    TEN_ASSERT(tcp_stream, "Invalid argument.");

    TEN_UNUSED int rc = uv_tcp_keepalive(tcp_stream->uv_stream, 1, 60);
    if (rc != 0) {
      TEN_ASSERT(0, "uv_tcp_keepalive() failed: %d", rc);
      // TEN_LOGW("uv_tcp_keepalive return %d", rc);
    }
  }
}

static int ten_transportbackend_tcp_connect(ten_transportbackend_t *backend,
                                            const ten_string_t *dest) {
  ten_string_t *host = NULL;
  ten_stream_t *stream = NULL;

  if (!backend || !dest || ten_string_is_empty(dest)) {
    // TEN_LOGE("Empty handle, treat as fail");
    goto error;
  }

  host = ten_uri_get_host(ten_string_get_raw_str(dest));
  if (!host) {
    // TEN_LOGE("Can not get ip info from uri %s", dest->buf);
    goto error;
  }

  uint16_t port = ten_uri_get_port(ten_string_get_raw_str(dest));
  if (port == 0) {
    // TEN_LOGE("Can not get port info from uri %s", dest->buf);
    goto error;
  }

  stream =
      ten_stream_tcp_create_uv(ten_runloop_get_raw(backend->transport->loop));
  TEN_ASSERT(stream, "Invalid argument.");
  stream->transport = backend->transport;

  uv_connect_t *req = (uv_connect_t *)TEN_MALLOC(sizeof(*req));
  TEN_ASSERT(req, "Failed to allocate memory.");
  if (!req) {
    // TEN_LOGE("Not enough memory");
    goto error;
  }

  memset(req, 0, sizeof(*req));

  struct sockaddr_in addr = {0};
  req->data = stream;
  TEN_UNUSED int rc = uv_ip4_addr(ten_string_get_raw_str(host), port, &addr);
  if (rc != 0) {
    // TEN_LOGE("uv_ip4_addr return %d", rc);
    TEN_ASSERT(0, "uv_ip4_addr() failed: %d", rc);
    goto error;
  }

  ten_string_destroy(host);
  host = NULL;

  ten_streambackend_tcp_t *tcp_stream =
      (ten_streambackend_tcp_t *)stream->backend;
  TEN_ASSERT(tcp_stream, "Invalid argument.");

  rc = uv_tcp_connect(req, tcp_stream->uv_stream, (struct sockaddr *)&addr,
                      on_server_connected);
  if (rc != 0) {
    // TEN_LOGE("uv_tcp_connect return %d", rc);
    TEN_ASSERT(0, "uv_tcp_connect() failed: %d", rc);
    goto error;
  }

  return rc;

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

  ten_transportbackend_tcp_t *backend =
      (ten_transportbackend_tcp_t *)server->data;
  ten_transport_t *transport = backend->base.transport;

  ten_stream_t *stream =
      ten_stream_tcp_create_uv(ten_runloop_get_raw(transport->loop));
  stream->transport = transport;

  uv_stream_t *uv_stream =
      ((ten_streambackend_tcp_t *)stream->backend)->uv_stream;

  int rc = uv_accept(server, uv_stream);
  if (rc != 0) {
    // TEN_LOGE("uv_accept return %d", rc);
    TEN_ASSERT(0, "uv_accept() failed: %d", rc);
    goto error;
  }

  ten_streambackend_tcp_dump_info((ten_streambackend_tcp_t *)stream->backend,
                                  "uv_accept() tcp stream: (^1:^2)");

  if (transport->on_client_accepted) {
    transport->on_client_accepted(transport, stream, status);
  }

  return;

error:
  if (stream) {
    ten_stream_close(stream);
  }
}

static int ten_transportbackend_tcp_listen(ten_transportbackend_t *backend,
                                           const ten_string_t *dest) {
  ten_transportbackend_tcp_t *self = (ten_transportbackend_tcp_t *)backend;

  if (self->server) {
    // TEN_LOGE("Listen socket already available");
    return -1;
  }

  uv_tcp_t *server = (uv_tcp_t *)TEN_MALLOC(sizeof(uv_tcp_t));
  TEN_ASSERT(server, "Failed to allocate memory.");

  memset(server, 0, sizeof(uv_tcp_t));

  int rc = uv_tcp_init(ten_runloop_get_raw(backend->transport->loop), server);
  TEN_ASSERT(!rc, "uv_tcp_init() failed: %d", rc);

  server->data = backend;
  self->server = server;

  ten_string_t *host = ten_uri_get_host(ten_string_get_raw_str(dest));
  uint16_t port = ten_uri_get_port(ten_string_get_raw_str(dest));
  struct sockaddr_in addr = {0};
  rc = uv_ip4_addr(ten_string_get_raw_str(host), port, &addr);
  if (rc != 0) {
    // TEN_LOGE("uv_ip4_addr return %d", rc);
    TEN_ASSERT(!rc, "uv_ip4_addr() failed: %d", rc);
  }

  rc = uv_tcp_bind(server, (const struct sockaddr *)&addr, 0);
  if (rc != 0) {
    // TEN_LOGE("uv_tcp_bind return %d", rc);
    TEN_ASSERT(!rc, "uv_tcp_bind() failed: %d", rc);
  }

  rc = uv_listen((uv_stream_t *)server, 8192, on_client_connected);
  if (rc != 0) {
    TEN_LOGE("Failed to uv_listen: %d", rc);
    TEN_ASSERT(0, "Failed to uv_listen()");
  }

  ten_string_destroy(host);

  return rc;
}

static ten_transportbackend_t *ten_transportbackend_tcp_create(
    ten_transport_t *transport, const ten_string_t *name) {
  ten_transportbackend_tcp_t *self = NULL;

  if (!name || !name->buf || !*name->buf) {
    // TEN_LOGE("Empty handle, treat as fail");
    goto error;
  }

  self = (ten_transportbackend_tcp_t *)TEN_MALLOC(
      sizeof(ten_transportbackend_tcp_t));
  TEN_ASSERT(self, "Failed to allocate memory.");
  if (!self) {
    // TEN_LOGE("Not enough memory");
    goto error;
  }

  memset(self, 0, sizeof(ten_transportbackend_tcp_t));

  ten_transportbackend_init(&self->base, transport, name);
  self->server = NULL;
  self->base.connect = ten_transportbackend_tcp_connect;
  self->base.listen = ten_transportbackend_tcp_listen;
  self->base.close = ten_transportbackend_tcp_close;

  return (ten_transportbackend_t *)self;

error:
  ten_transportbackend_tcp_destroy(self);
  return NULL;
}

const ten_transportbackend_factory_t uv_tp_backend_tcp = {
    .create = ten_transportbackend_tcp_create,
};
