//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/connection/connection.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_connection_t ten_connection_t;
typedef struct ten_engine_t ten_engine_t;

/**
 * First of all, the connection migration happens only if it receives the first
 * message from the related implementation protocol. The connection attaches to
 * the app at this time.
 *
 * < protocol::in_msgs queue >
 *  | MSG N | ... | MSG 1 |
 *                  ___
 *                   |
 *                   --> Sends the first msg to app's queue.
 *                     |
 *                     -->  Do connection migration. ---
 *                                                     |
 *                                                     |--> Migration is done.
 *
 * After the migration is completed, the connection attaches to the engine or
 * the remote, and then all the messages in the protocol::in_msgs queue could be
 * delivered to the engine's queue.
 *
 * So, one of the specifications of the protocol implementation is to ensure
 * that only one message is delivered to the connection (i.e., calling
 * 'ten_connection_on_msgs()') _before_ the connection migration is totally
 * completed.
 *
 * The processing of the connection migration is expected to be as follows.
 *
 *    |     APP thread      | External protocol thread |    Engine thread   |
 * -----------------------------------------------------------------------------
 * |   connection::migrate()
 * |
 * t    protocol::migrate()
 * i
 * m                         implementation::migrate()
 * e
 *                                                       protocol::on_migrated()
 * l                                          connection::on_protocol_migrated()
 * i
 * n   connection::clean()
 * e    protocol::clean()
 * |
 * |                         implementation::clean()
 * |
 * |                                                      protocol::on_cleaned()
 * |                                           connection::on_protocol_cleaned()
 * |
 * |                         implementation::on_cleaned()
 * V
 *
 * The pseudo-codes of above stages are as follows.
 *
 * - connection::migrate() {
 *       /// Calling in the app thread.
 *
 *       connection::migration_state = DOING;
 *       protocol::migrate();
 *   }
 *
 * - protocol::migrate() {
 *       /// Calling in the app thread.
 *
 *       /// Switch to the external protocol thread to do the migration.
 *       protocol::impl_notify(implementation::migrate);
 *   }
 *
 * - implementation::migrate() {
 *      /// Calling in the external protocol thread.
 *
 *      /// Do the migration. Ex: stop the stream to read the data. After than,
 *      /// switch to the engine thread to do the post processing of the
 *      /// migration.
 *      ten_runloop_post_task(engine_thread, protocol::on_migrated);
 *   }
 *
 * - protocol::on_migrated() {
 *      /// Calling in the engine thread.
 *
 *      connection::on_protocol_migrated();
 *   }
 *
 * - connection::on_protocol_migrated() {
 *      /// Calling in the engine thread.
 *
 *      /// The connection has been migrated, switch to the app thread to do
 *      /// some cleanup. Note that the corresponding resources of the
 *      /// connection are created in the app thread, so the cleanup must be
 *      /// started in the app thread too.
 *      ten_runloop_post_task(app_thread, connection::clean);
 *   }
 *
 * - connection::clean() {
 *      /// Calling in the app thread.
 *
 *      ten_app_del_orphaned_connection(connection);
 *      protocol::clean();
 *   }
 *
 * - protocol::clean() {
 *      /// Calling in the app thread.
 *
 *      /// Switch to the external protocol thread to do the cleanup.
 *      protocol::impl_notify(implementation::clean);
 *   }
 *
 * - implementation::clean() {
 *      /// Calling in the external protocol thread.
 *
 *      /// Do the cleanup, ex: close the stream. After than, switch to the
 *      /// engine thread to do the post processing of cleanup.
 *      ten_runloop_post_task(engine_thread, protocol::on_cleaned);
 *   }
 *
 * - protocol::on_cleaned() {
 *       /// Calling in the engine thread.
 *
 *       /// Update the belonging thread to the engine thread.
 *
 *       connection::on_protocol_cleaned();
 *   }
 *
 * - connection::on_protocol_cleaned() {
 *       /// Calling in the engine thread.
 *
 *       connection::migration_state = DONE;
 *       connection::attach_to = ENGINE;
 *
 *       /// Update the belonging thread to the engine thread.
 *
 *       /// After then, notify the external protocol thread that all the
 *       /// migration has been completed.
 *       protocol::impl_notify(implementation::on_cleaned);
 *   }
 *
 * - implementation::on_cleaned() {
 *       /// Calling in the external protocol thread.
 *
 *       /// All the migration is completed, continue to handle the pending
 *       /// msgs.
 *       protocol::migration_state = DONE;
 *
 *       /// It's safe to retrieve the attached runloop of the base protocol.
 *   }
 */
TEN_RUNTIME_PRIVATE_API void ten_connection_migrate(ten_connection_t *self,
                                                    ten_engine_t *engine,
                                                    ten_shared_ptr_t *cmd);

/**
 * @brief Check if the connection needs to be migrated first before handling
 * TEN messages.
 */
TEN_RUNTIME_PRIVATE_API bool ten_connection_needs_to_migrate(
    ten_connection_t *self, ten_engine_t *engine);

/**
 * @brief Once the migration is done, the connection 'attach_to' the engine.
 */
TEN_RUNTIME_PRIVATE_API void ten_connection_upgrade_migration_state_to_done(
    ten_connection_t *self, ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API void
ten_connection_migration_state_reset_when_engine_not_found(
    ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API TEN_CONNECTION_MIGRATION_STATE
ten_connection_get_migration_state(ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void ten_connection_set_migration_state(
    ten_connection_t *self, TEN_CONNECTION_MIGRATION_STATE new_state);
