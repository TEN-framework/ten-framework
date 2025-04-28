//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/connection/migration.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/migration.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/macro/check.h"

void ten_connection_migrate(ten_connection_t *self, ten_engine_t *engine,
                            ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(self, true), "Should not happen.");
  // Call in the app thread.
  TEN_ASSERT(ten_app_check_integrity(engine->app, true), "Should not happen.");
  TEN_ASSERT(self->protocol, "Should not happen.");

  ten_protocol_migrate(self->protocol, engine, self, cmd, NULL);
}

bool ten_connection_needs_to_migrate(ten_connection_t *self,
                                     ten_engine_t *engine) {
  TEN_ASSERT(self, "Invalid argument.");

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is always called in the app thread before
  // handling the cmd. If the connection needs to migrate, it means the app and
  // engine thread are different, and the belonging thread of the connection
  // will be switched from the app thread to the engine thread. So we do _not_
  // check thread safety here.
  TEN_ASSERT(ten_connection_check_integrity(self, false), "Invalid argument.");

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is always called in the app thread, and the
  // engine may have its own thread, so we do not check thread safety here. As
  // the 'ten_engine_t::has_own_loop' is immutable, there is no access issues.
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Invalid argument.");

  if (engine->has_own_loop) {
    return self->migration_state == TEN_CONNECTION_MIGRATION_STATE_FIRST_MSG;
  }

  // The engine does not have its own thread, it's time to upgrade the
  // migration_state to 'DONE' directly.
  if (self->migration_state == TEN_CONNECTION_MIGRATION_STATE_FIRST_MSG) {
    TEN_ASSERT(ten_connection_attach_to(self) == TEN_CONNECTION_ATTACH_TO_APP,
               "The connection still attaches to the app before migration.");

    ten_app_t *app = self->attached_target.app;
    TEN_ASSERT(
        app && ten_app_check_integrity(app, true),
        "This function is called from the app thread before handling the cmd.");

    ten_app_del_orphan_connection(app, self);
    ten_connection_upgrade_migration_state_to_done(self, engine);
  }

  return false;
}

static void ten_protocol_on_cleaned_task(void *self_, void *arg) {
  ten_protocol_t *self = (ten_protocol_t *)self_;
  TEN_ASSERT(self && ten_protocol_check_integrity(self, true),
             "Access across threads.");

  bool is_migration_state_reset = arg;

  self->on_cleaned_for_external(self, is_migration_state_reset);

  ten_ref_dec_ref(&self->ref);
}

static void ten_connection_on_migration_is_done_or_reset(
    ten_connection_t *self, bool is_migration_state_reset) {
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "Access across threads.");

  ten_protocol_t *protocol = self->protocol;
  TEN_ASSERT(protocol && ten_protocol_check_integrity(protocol, true),
             "Access across threads.");

  if (protocol->on_cleaned_for_external) {
    ten_ref_inc_ref(&protocol->ref);

    // The connection migration is completed, it's time to notify the
    // implementation protocol to do the post processing (ex: continue to handle
    // the messages if there are any messages received during the connection
    // migration) by calling 'ten_protocol_t::on_cleaned_for_external()'. But
    // we can not call this function directly because of the following 2
    // reasons:
    //
    // 1) The engine has not started to handle the first message from the
    //    connection after the migration is completed (the message delivered to
    //    the connection before the migration has started), refer to
    //    'ten_engine_on_connection_cleaned()'. In other words, the
    //    corresponding 'ten_remote_t' for the connection has not been created
    //    yet.
    //
    // 2) The implementation protocol who implements the _integrated_ protocol
    //    interface attaches to the engine's runloop if the migration is
    //    completed. If we call 'ten_protocol_t::on_cleaned_for_external()'
    //    directly and there are messages pending during the migration, those
    //    messages will be handled _before_ the first message, in other words,
    //    the messages handled by the engine are out of order.
    //
    // So we prefer to use a runloop task here even through we are in the engine
    // thread if the migration is completed. We must make sure the engine handle
    // all the messages in the original order.
    ten_runloop_t *loop = ten_connection_get_attached_runloop(self);
    int rc =
        ten_runloop_post_task_tail(loop, ten_protocol_on_cleaned_task, protocol,
                                   (void *)is_migration_state_reset);
    TEN_ASSERT(!rc, "Should not happen.");
  } else {
    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_connection_upgrade_migration_state_to_done(ten_connection_t *self,
                                                    ten_engine_t *engine) {
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "The migration is completed, the belonging thread must be the "
             "engine's thread.");

  if (engine) {
    // The message is sent to the app, not an engine.

    TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
               "Access across threads.");

    // @{
    // Attach to engine.
    self->attached_target.engine = engine;
    ten_atomic_store(&self->attach_to, TEN_CONNECTION_ATTACH_TO_ENGINE);

    ten_engine_add_orphan_connection(engine, self);
    // @}
  }

  self->migration_state = TEN_CONNECTION_MIGRATION_STATE_DONE;

  ten_connection_on_migration_is_done_or_reset(self, false);
}

void ten_connection_migration_state_reset_when_engine_not_found(
    ten_connection_t *self) {
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "This function is always called from the app thread when the "
             "expected engine was not found.");
  TEN_ASSERT(
      ten_connection_attach_to(self) == TEN_CONNECTION_ATTACH_TO_APP,
      "No engine has been matched yet, the connection still attaches to the "
      "app now.");

  self->migration_state = TEN_CONNECTION_MIGRATION_STATE_INIT;

  ten_connection_on_migration_is_done_or_reset(self, true);
}

TEN_CONNECTION_MIGRATION_STATE
ten_connection_get_migration_state(ten_connection_t *self) {
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "Access across threads.");

  return self->migration_state;
}

void ten_connection_set_migration_state(
    ten_connection_t *self, TEN_CONNECTION_MIGRATION_STATE new_state) {
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "Access across threads.");

  self->migration_state = new_state;
}
