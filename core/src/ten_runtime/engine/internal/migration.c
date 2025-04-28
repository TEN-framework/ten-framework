//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/internal/migration.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static ten_engine_migration_user_data_t *ten_engine_migration_user_data_create(
    ten_connection_t *connection, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(connection && cmd, "Invalid argument.");

  ten_engine_migration_user_data_t *self =
      TEN_MALLOC(sizeof(ten_engine_migration_user_data_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->connection = connection;
  self->cmd = ten_shared_ptr_clone(cmd);

  return self;
}

static void ten_engine_migration_user_data_destroy(
    ten_engine_migration_user_data_t *self) {
  TEN_ASSERT(self && self->cmd, "Invalid argument.");

  self->connection = NULL;

  ten_shared_ptr_destroy(self->cmd);
  self->cmd = NULL;

  TEN_FREE(self);
}

void ten_engine_on_connection_cleaned(ten_engine_t *self,
                                      ten_connection_t *connection,
                                      ten_shared_ptr_t *cmd) {
  TEN_ASSERT(connection, "Invalid argument.");

  ten_protocol_t *protocol = connection->protocol;
  TEN_ASSERT(protocol, "Invalid argument.");

  TEN_UNUSED int rc = ten_event_wait(connection->is_cleaned,
                                     TIMEOUT_FOR_CONNECTION_ALL_CLEANED);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &connection->thread_check);
  TEN_ASSERT(ten_connection_check_integrity(connection, true),
             "Access across threads.");

  ten_protocol_update_belonging_thread_on_cleaned(protocol);

  // Because the command is from externally (ex: clients or other engines),
  // there will be some things the engine needs to do to handle this command, so
  // we need to put it into the queue for external commands first, rather than
  // enabling the engine to handle this command directly at this time point.
  //
  // i.e., should not call `ten_engine_handle_msg(self, cmd);` directly here.
  ten_engine_append_to_in_msgs_queue(self, cmd);

  // This is the last stage of the connection migration process, the
  // implementation protocol will be notified to do some things (ex: continue to
  // handle messages if there are any messages received during the connection
  // migration) by the following function. And
  // 'ten_connection_upgrade_migration_state_to_done()' _must_ be called after
  // the above 'ten_engine_push_to_external_cmds_queue()', as the messages must
  // be handled by engine in their original order.
  ten_connection_upgrade_migration_state_to_done(connection, self);
}

static void ten_engine_on_connection_cleaned_task(void *self_, void *arg) {
  ten_engine_t *self = (ten_engine_t *)self_;
  TEN_ASSERT(self && ten_engine_check_integrity(self, true),
             "Access across threads.");

  ten_engine_migration_user_data_t *user_data =
      (ten_engine_migration_user_data_t *)arg;
  TEN_ASSERT(user_data, "Invalid argument.");

  ten_shared_ptr_t *cmd = user_data->cmd;
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  ten_connection_t *connection = user_data->connection;
  TEN_ASSERT(connection, "Invalid argument.");

  ten_engine_on_connection_cleaned(self, connection, cmd);

  ten_engine_migration_user_data_destroy(user_data);

  ten_ref_dec_ref(&self->ref);
}

void ten_engine_on_connection_cleaned_async(ten_engine_t *self,
                                            ten_connection_t *connection,
                                            ten_shared_ptr_t *cmd) {
  TEN_ASSERT(
      self &&
          // TEN_NOLINTNEXTLINE(thread-check)
          ten_engine_check_integrity(self, false),
      "This function is intended to be called outside of the engine thread.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Access across threads.");
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  ten_engine_migration_user_data_t *user_data =
      ten_engine_migration_user_data_create(connection, cmd);

  ten_ref_inc_ref(&self->ref);

  int rc = ten_runloop_post_task_tail(ten_engine_get_attached_runloop(self),
                                      ten_engine_on_connection_cleaned_task,
                                      self, user_data);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);
    ten_ref_dec_ref(&self->ref);

    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_engine_on_connection_closed(ten_connection_t *connection,
                                     TEN_UNUSED void *user_data) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The connection only attaches to the engine before the
  // corresponding 'ten_remote_t' object is created, which should be an
  // intermediate state. It means the protocol has been closed if this function
  // is called, and the engine thread might be ended, so we do not check thread
  // integrity here.
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, false),
             "Invalid argument.");

  ten_connection_destroy(connection);
}
