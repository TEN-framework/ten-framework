//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/endpoint.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/protocol/close.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

static ten_connection_t *create_connection_when_client_accepted(
    ten_protocol_t *listening_protocol, ten_protocol_t *protocol) {
  TEN_ASSERT(listening_protocol &&
                 ten_protocol_check_integrity(listening_protocol, true),
             "Should not happen.");
  TEN_ASSERT(protocol && ten_protocol_check_integrity(protocol, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_attach_to(protocol) == TEN_PROTOCOL_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *app = protocol->attached_target.app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  TEN_LOGD("[%s] A client is connected.", ten_app_get_uri(app));

  // Create a connection to represent it.
  ten_connection_t *connection = ten_connection_create(protocol);
  TEN_ASSERT(connection, "Should not happen.");

  ten_connection_attach_to_app(connection, app);

  return connection;
}

bool ten_app_endpoint_listen(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (!self->endpoint_protocol) {
    return false;
  }

  ten_protocol_listen(self->endpoint_protocol,
                      ten_string_get_raw_str(&self->uri),
                      create_connection_when_client_accepted);

  return true;
}

bool ten_app_is_endpoint_closed(ten_app_t *self) {
  TEN_ASSERT(self && ten_app_check_integrity(self, true),
             "Access across threads.");

  if (!self->endpoint_protocol) {
    return true;
  }

  return self->endpoint_protocol->state == TEN_PROTOCOL_STATE_CLOSED;
}
