//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/connection/connection.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/migration.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_connection_check_integrity(ten_connection_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_CONNECTION_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

/**
 * @brief Checks if a connection can be closed.
 *
 * This function determines whether a connection is in a state where it can be
 * safely closed. A connection can be closed if it has no associated protocol or
 * if its protocol is already in a closed state.
 *
 * @param self The connection to check.
 * @return true if the connection can be closed, false otherwise.
 */
static bool ten_connection_could_be_close(ten_connection_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  if (!self->protocol || self->protocol->state == TEN_PROTOCOL_STATE_CLOSED) {
    // If there is no protocol, or the protocol has already been closed, then
    // this 'connection' could be closed, too.
    return true;
  }

  return false;
}

/**
 * @brief Destroys a connection instance and frees all associated resources.
 *
 * This function should only be called after the connection has been properly
 * closed (state == TEN_CONNECTION_STATE_CLOSED). It releases all resources
 * associated with the connection, including its URI string, protocol reference,
 * and event objects.
 *
 * Note: This function does not perform thread safety checks as it's typically
 * called during cleanup when the owning thread may have already terminated.
 *
 * @param self Pointer to the connection instance to be destroyed.
 */
void ten_connection_destroy(ten_connection_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(
                 self,
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The belonging thread of the 'connection' is
                 // ended when this function is called, so we can not check
                 // thread integrity here.
                 false),
             "Should not happen.");
  TEN_ASSERT(self->state == TEN_CONNECTION_STATE_CLOSED,
             "Connection should be closed completely before been destroyed.");

  ten_signature_set(&self->signature, 0);

  ten_string_deinit(&self->uri);

  if (self->protocol) {
    ten_ref_dec_ref(&self->protocol->ref);
  }

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_event_destroy(self->is_cleaned);

  TEN_FREE(self);
}

/**
 * @brief Performs the actual closing operations for a connection.
 *
 * This function is called when all resources associated with the connection
 * have been properly released and the connection is ready to be fully closed.
 * It changes the connection state to CLOSED and invokes the registered
 * `on_closed` callback to notify the owner of the connection.
 *
 * Note: This function should only be called after verifying that the connection
 * can be safely closed using `ten_connection_could_be_close()`.
 *
 * @param self Pointer to the connection to be closed.
 */
static void ten_connection_do_close(ten_connection_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  // For now, the 'on_closed' callback could not be NULL, otherwise the
  // connection would not be destroyed.
  TEN_ASSERT(self->on_closed, "Should not happen.");

  // Mark the connection as CLOSED, which serves as a signal to other modules
  // (e.g., remote, app) that this connection is no longer active and should not
  // be used. This state change is critical for proper resource management and
  // prevents attempts to use a closed connection.
  self->state = TEN_CONNECTION_STATE_CLOSED;

  // Call the registered `on_closed` callback.
  self->on_closed(self, self->on_closed_data);
}

/**
 * @brief Handles the closing process of a connection.
 *
 * This function is called when resources associated with the connection (such
 * as the protocol) have been closed. It checks if all resources are properly
 * released using `ten_connection_could_be_close()`. If the connection can be
 * closed, it proceeds with `ten_connection_do_close()`.
 *
 * Note: This function is part of the bottom-up notification process in
 * the connection closing sequence, similar to the protocol closing process.
 *
 * @param self Pointer to the connection to be closed.
 */
static void ten_connection_on_close(ten_connection_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  if (ten_connection_could_be_close(self) == false) {
    TEN_LOGD("[%s] Could not close alive connection.",
             ten_string_get_raw_str(&self->uri));
    return;
  }

  TEN_LOGD("[%s] Connection can be closed now.",
           ten_string_get_raw_str(&self->uri));

  ten_connection_do_close(self);
}

/**
 * @brief Closes a connection.
 *
 * This function initiates the closing process for a connection. If the
 * connection is already in the process of closing, the function will return
 * without taking any action. Otherwise, it marks the connection as closing and
 * proceeds to close the underlying protocol if it exists and is not already
 * closed. If the protocol is already closed, it proceeds directly to close the
 * connection.
 *
 * @param self Pointer to the connection to be closed.
 */
void ten_connection_close(ten_connection_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  if (self->state >= TEN_CONNECTION_STATE_CLOSING) {
    TEN_LOGD("Connection is closing, do not close again.");
    return;
  }

  TEN_LOGD("Try to close connection.");

  self->state = TEN_CONNECTION_STATE_CLOSING;

  ten_protocol_t *protocol = self->protocol;
  if (protocol && protocol->state != TEN_PROTOCOL_STATE_CLOSED) {
    // The protocol still exists, close it first.
    ten_protocol_close(protocol);
  } else {
    // The protocol has been closed, proceed to close the connection directly.
    ten_connection_on_close(self);
  }
}

/**
 * @brief Callback function invoked when a protocol is closed.
 *
 * This function is registered as the callback for protocol closure events.
 * When a protocol closes, this function is called to handle the connection's
 * response to that event. Depending on the connection's current state:
 * - If the connection is already in CLOSING state, it continues the connection
 *   closure process (bottom-up notification chain).
 * - If the connection is in any other state, it initiates the connection
 *   closure process since the underlying protocol has closed unexpectedly.
 *
 * @param protocol The protocol that has been closed (unused but kept for
 * callback signature).
 * @param on_closed_data User data passed during callback registration, which is
 * the connection object.
 */
void ten_connection_on_protocol_closed(TEN_UNUSED ten_protocol_t *protocol,
                                       void *on_closed_data) {
  ten_connection_t *self = (ten_connection_t *)on_closed_data;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);
  TEN_ASSERT(self->state < TEN_CONNECTION_STATE_CLOSED, "Should not happen.");

  if (self->state == TEN_CONNECTION_STATE_CLOSING) {
    // The connection is already in CLOSING state, which means the closure was
    // initiated by the TEN runtime (top-down closure chain), e.g.:
    // app => engine => remote => connection => protocol.
    // Now that the protocol has closed (bottom-up notification), we can
    // continue with closing this connection and notify our parent in the chain.
    ten_connection_on_close(self);
  } else {
    // The connection is in any other state, which means the closure was
    // initiated by the protocol (bottom-up notification), e.g.:
    // app => engine => remote => connection => protocol.
    // Now that the protocol has closed unexpectedly, we need to close the
    // connection directly.
    ten_connection_close(self);
  }
}

ten_connection_t *ten_connection_create(ten_protocol_t *protocol) {
  TEN_ASSERT(protocol, "Should not happen.");

  ten_connection_t *self =
      (ten_connection_t *)TEN_MALLOC(sizeof(ten_connection_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_CONNECTION_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  ten_atomic_store(&self->attach_to, TEN_CONNECTION_ATTACH_TO_INVALID);
  self->attached_target.app = NULL;
  self->attached_target.remote = NULL;

  self->migration_state = TEN_CONNECTION_MIGRATION_STATE_INIT;

  TEN_STRING_INIT(self->uri);

  self->state = TEN_CONNECTION_STATE_INIT;

  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->is_cleaned = ten_event_create(0, 0);

  self->protocol = protocol;
  ten_protocol_attach_to_connection(self->protocol, self);

  self->duplicate = false;

  TEN_LOGD("Create a connection %p", self);

  return self;
}

void ten_connection_set_on_closed(ten_connection_t *self,
                                  ten_connection_on_closed_func_t on_closed,
                                  void *on_closed_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(on_closed, "Should not happen.");

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}

void ten_connection_send_msg(ten_connection_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(self, true), "Should not happen.");

  // The message sends to connection channel MUST have dest loc.
  TEN_ASSERT(msg && ten_msg_get_dest_cnt(msg) == 1, "Should not happen.");

  if (self->state >= TEN_CONNECTION_STATE_CLOSING) {
    TEN_LOGD("Connection is closing, do not send msgs.");
    return;
  }

  ten_protocol_send_msg(self->protocol, msg);
}

static bool ten_connection_on_input(ten_connection_t *self,
                                    ten_shared_ptr_t *msg, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  // A 'connection' must be attached to an engine or a app. The way to attaching
  // to an engine is through a remote.

  switch (ten_atomic_load(&self->attach_to)) {
  case TEN_CONNECTION_ATTACH_TO_REMOTE:
    // Enable the 'remote' to handle this message.
    return ten_remote_on_input(self->attached_target.remote, msg, err);
  case TEN_CONNECTION_ATTACH_TO_APP:
    // Enable the 'app' to handle this message.
    return ten_app_handle_in_msg(self->attached_target.app, self, msg, err);
  default:
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
}

static void ten_connection_on_protocol_cleaned(ten_protocol_t *protocol) {
  TEN_ASSERT(protocol, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "We are in the app thread, and 'protocol' still belongs to the "
             "app thread now.");

  ten_connection_t *connection = protocol->attached_target.connection;
  TEN_ASSERT(connection, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(connection, true),
             "Invalid use of connection %p. We are in the app thread, and "
             "'connection' still belongs to the app thread now.",
             connection);

  ten_event_set(connection->is_cleaned);
}

void ten_connection_clean(ten_connection_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // The connection belongs to the app thread initially, and will be transferred
  // to the engine thread after the migration. But now (before the 'cleaning'),
  // the connection belongs to the app thread, and this function is called in
  // the app thread, so we can perform thread checking here.
  TEN_ASSERT(self && ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(self->attach_to == TEN_CONNECTION_ATTACH_TO_APP,
             "Invalid argument.");
  TEN_ASSERT(self->attached_target.app &&
                 ten_app_check_integrity(self->attached_target.app, true),
             "This function is called in the app thread");

  // The only thing which a connection needs to clean is the containing
  // protocol.
  ten_protocol_clean(self->protocol, ten_connection_on_protocol_cleaned);
}

static void ten_connection_handle_command_from_external_client(
    ten_connection_t *self, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Invalid argument.");

  // The command is coming from the outside of the TEN world, generate a
  // command ID for it.
  const char *cmd_id = ten_cmd_base_gen_new_cmd_id_forcibly(cmd);

  const char *src_uri = ten_msg_get_src_app_uri(cmd);
  TEN_ASSERT(src_uri, "Should not happen.");

  // If this message is coming from the outside of the TEN world (i.e.,
  // a client), regardless of whether the src_uri of the command is set or
  // not, we forcibly use the command ID as the identity of that client.
  //
  // The effect of this operation is that when the corresponding remote is
  // created, the URI of that remote will be the source URI of the first
  // command it receives.
  ten_msg_set_src_uri(cmd, cmd_id);

  ten_protocol_t *protocol = self->protocol;
  TEN_ASSERT(protocol && ten_protocol_check_integrity(protocol, true),
             "Access across threads.");

  protocol->role = TEN_PROTOCOL_ROLE_IN_EXTERNAL;
}

void ten_connection_on_msgs(ten_connection_t *self, ten_list_t *msgs) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(msgs, "Should not happen.");

  // Do some thread-safety checking.
  switch (ten_connection_attach_to(self)) {
  case TEN_CONNECTION_ATTACH_TO_APP:
    TEN_ASSERT(ten_app_check_integrity(self->attached_target.app, true),
               "Should not happen.");
    break;
  case TEN_CONNECTION_ATTACH_TO_REMOTE:
    TEN_ASSERT(
        ten_engine_check_integrity(self->attached_target.remote->engine, true),
        "Should not happen.");
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_list_foreach (msgs, iter) {
    ten_shared_ptr_t *msg = ten_smart_ptr_listnode_get(iter.node);

    if (ten_msg_is_cmd_and_result(msg)) {
      // If this command is coming from outside of the TEN world (i.e.,
      // clients), the command ID would be empty, so we generate a new one for
      // it in this case now.
      const char *cmd_id = ten_cmd_base_get_cmd_id(msg);
      TEN_ASSERT(cmd_id, "Should not happen.");

      if (strlen(cmd_id) == 0) {
        ten_connection_handle_command_from_external_client(self, msg);
      }
    } else {
      if (ten_atomic_load(&self->attach_to) !=
          TEN_CONNECTION_ATTACH_TO_REMOTE) {
        // For a non-command message, if the connection isn't attached to a
        // engine, the message is nowhere to go, therefore, what we can do is to
        // drop them directly.
        continue;
      }
    }

    // If this connection has not been assigned a URI yet, the source URI of the
    // first received command will become the URI of this connection.
    if (ten_string_is_empty(&self->uri)) {
      ten_string_set_from_c_str(&self->uri, ten_msg_get_src_app_uri(msg));
    }

    // Send into the TEN runtime to be processed.
    ten_connection_on_input(self, msg, &err);
  }

  ten_error_deinit(&err);
}

/**
 * @brief Initiates a connection to a remote server using the connection's
 * protocol.
 *
 * This function attempts to establish a connection to a remote server specified
 * by the URI. It verifies that the connection is valid and has a communication
 * protocol attached. If the connection is already attached to a remote, it
 * ensures the remote's engine is valid. The callback will be invoked when the
 * connection attempt completes, indicating success or failure.
 *
 * @param self The connection instance initiating the connection.
 * @param uri The URI of the remote server to connect to. Must not be NULL.
 * @param on_server_connected Callback function to be invoked when the
 * connection attempt completes. The first parameter is the protocol instance,
 * and the second parameter indicates success (true) or failure (false).
 */
void ten_connection_connect_to(ten_connection_t *self, const char *uri,
                               void (*on_server_connected)(ten_protocol_t *,
                                                           bool)) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(uri, "URI cannot be NULL.");

  // If already attached to a remote, verify the remote's engine integrity.
  if (ten_atomic_load(&self->attach_to) == TEN_CONNECTION_ATTACH_TO_REMOTE) {
    TEN_ASSERT(
        ten_engine_check_integrity(self->attached_target.remote->engine, true),
        "Remote engine integrity check failed.");
  }

  // Verify protocol exists and is valid for communication.
  TEN_ASSERT(self->protocol, "Connection must have a valid protocol.");
  TEN_ASSERT(ten_protocol_check_integrity(self->protocol, true),
             "Connection must have a valid protocol.");
  TEN_ASSERT(ten_protocol_role_is_communication(self->protocol),
             "Protocol must be a communication protocol.");

  // Delegate the connection request to the protocol layer.
  ten_protocol_connect_to(self->protocol, uri, on_server_connected);
}

void ten_connection_attach_to_remote(ten_connection_t *self,
                                     ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(remote && ten_remote_check_integrity(remote, true),
             "Should not happen.");

  ten_atomic_store(&self->attach_to, TEN_CONNECTION_ATTACH_TO_REMOTE);
  self->attached_target.remote = remote;

  if (self->protocol) {
    ten_protocol_set_uri(self->protocol, &remote->uri);
  }
}

void ten_connection_attach_to_app(ten_connection_t *self, ten_app_t *app) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_atomic_store(&self->attach_to, TEN_CONNECTION_ATTACH_TO_APP);
  self->attached_target.app = app;

  // This connection has not been attached to a remote, so we record this to
  // prevent resource leak when we perform garbage collection.
  ten_app_add_orphan_connection(app, self);
}

TEN_CONNECTION_ATTACH_TO ten_connection_attach_to(ten_connection_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function will be called from the protocol thread, so we
  // use 'ten_atomic_t' here.
  TEN_ASSERT(self && ten_connection_check_integrity(self, false),
             "Should not happen.");
  return ten_atomic_load(&self->attach_to);
}

ten_runloop_t *ten_connection_get_attached_runloop(ten_connection_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in
                 // different threads.
                 ten_connection_check_integrity(self, false),
             "Should not happen.");

  // This function will be called from the implementation protocol thread (ex:
  // 'ten_protocol_asynced_on_input_async()'), and the
  // 'ten_connection_t::migration_state' must be only accessed from the TEN
  // world, so do _not_ check 'ten_connection_t::migration_state' here.
  //
  // The caller side must be responsible for calling this function at the right
  // time (i.e., when the first message received which means the migration has
  // not started yet or the 'ten_protocol_t::on_cleaned_for_external()'
  // function has been called which means the migration is completed). Refer to
  // 'ten_protocol_asynced_on_input_async()'.

  switch (ten_connection_attach_to(self)) {
  case TEN_CONNECTION_ATTACH_TO_REMOTE:
    return ten_remote_get_attached_runloop(self->attached_target.remote);
  case TEN_CONNECTION_ATTACH_TO_ENGINE:
    return ten_engine_get_attached_runloop(self->attached_target.engine);
  case TEN_CONNECTION_ATTACH_TO_APP:
    return ten_app_get_attached_runloop(self->attached_target.app);
  default:
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

void ten_connection_reply_result_for_duplicate_connection(
    ten_connection_t *self, ten_shared_ptr_t *cmd_start_graph) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(self, true),
             "Invalid use of connection %p.", self);

  TEN_ASSERT(cmd_start_graph, "Invalid argument.");
  TEN_ASSERT(ten_cmd_base_check_integrity(cmd_start_graph),
             "Invalid use of cmd %p.", cmd_start_graph);

  self->duplicate = true;

  ten_shared_ptr_t *ret_cmd =
      ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_OK, cmd_start_graph);
  ten_msg_set_property(ret_cmd, TEN_STR_DETAIL,
                       ten_value_create_string(TEN_STR_DUPLICATE), NULL);
  ten_msg_clear_and_set_dest_from_msg_src(ret_cmd, cmd_start_graph);
  ten_connection_send_msg(self, ret_cmd);
  ten_shared_ptr_destroy(ret_cmd);
}
