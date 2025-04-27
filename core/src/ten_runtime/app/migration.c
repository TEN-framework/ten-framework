//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/migration.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_app_clean_connection(ten_app_t *self, ten_connection_t *connection) {
  TEN_ASSERT(self && ten_app_check_integrity(self, true),
             "This function is called from the app thread when the protocol "
             "has been migrated.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "The connection still belongs to the app thread before cleaning.");
  TEN_ASSERT(
      ten_connection_attach_to(connection) == TEN_CONNECTION_ATTACH_TO_APP,
      "The connection still attaches to the app before cleaning.");

  ten_app_del_orphan_connection(self, connection);
  ten_connection_clean(connection);
}

static void ten_app_clean_connection_task(void *connection_,
                                          TEN_UNUSED void *arg) {
  ten_connection_t *connection = (ten_connection_t *)connection_;
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  TEN_ASSERT(
      ten_connection_attach_to(connection) == TEN_CONNECTION_ATTACH_TO_APP,
      "The connection still attaches to the app before cleaning.");

  ten_app_t *app = connection->attached_target.app;
  TEN_ASSERT(app && ten_app_check_integrity(app, true),
             "Access across threads.");

  ten_app_clean_connection(app, connection);
}

// The relevant resource of a connection might be bound to the event loop of the
// app, so the app thread is responsible for cleaning them up.
void ten_app_clean_connection_async(ten_app_t *self,
                                    ten_connection_t *connection) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called outside
                 // of the TEN app thread.
                 ten_app_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(connection &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: 'connection' belongs to app thread, but this
                 // function is intended to be called outside of the app thread.
                 ten_connection_check_integrity(connection, false),
             "Should not happen.");

  int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(self),
                                      ten_app_clean_connection_task, connection,
                                      NULL);
  TEN_ASSERT(!rc, "Should not happen.");
}
