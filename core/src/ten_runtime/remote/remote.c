//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/remote/remote.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/remote_interface.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_remote_check_integrity(ten_remote_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_REMOTE_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

static bool ten_remote_could_be_close(ten_remote_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  if (!self->connection ||
      self->connection->state == TEN_CONNECTION_STATE_CLOSED) {
    // The 'connection' has already been closed, so this 'remote' could be
    // closed, too.
    return true;
  }
  return false;
}

/**
 * @brief Destroys a remote instance and frees all associated resources.
 *
 * This function should only be called after the remote has been properly closed
 * (state == TEN_REMOTE_STATE_CLOSED). It releases all resources associated with
 * the remote, including its connection, URI string, and any pending commands.
 *
 * Note: This function does not perform thread safety checks as it's typically
 * called during cleanup when the owning thread may have already terminated.
 *
 * @param self Pointer to the remote instance to be destroyed.
 */
void ten_remote_destroy(ten_remote_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: The belonging thread of the 'remote' is ended when
      // this function is called, so we can not check thread integrity here.
      ten_remote_check_integrity(self, false), "Should not happen.");
  TEN_ASSERT(self->state == TEN_REMOTE_STATE_CLOSED,
             "Remote should be closed first before been destroyed.");

  ten_signature_set(&self->signature, 0);

  if (self->connection) {
    ten_connection_destroy(self->connection);
  }

  ten_string_deinit(&self->uri);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  if (self->on_server_connected_cmd) {
    ten_shared_ptr_destroy(self->on_server_connected_cmd);
  }

  TEN_FREE(self);
}

/**
 * @brief Finalizes the closing process of a remote.
 *
 * This function is called when all resources associated with the remote have
 * been properly released and it's safe to complete the closing process. It
 * marks the remote as closed and notifies any registered callbacks.
 *
 * This function is part of the bottom-up notification process in the remote
 * closing sequence. It's called by `ten_remote_on_close()` after verifying
 * that all resources are properly released.
 *
 * @param self Pointer to the remote to be closed.
 */
static void ten_remote_do_close(ten_remote_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  // Mark this 'remote' as CLOSED, which indicates that all resources have been
  // properly released and the remote is fully closed. This state change allows
  // other modules to safely interact with or clean up the remote object.
  self->state = TEN_REMOTE_STATE_CLOSED;

  if (self->on_closed) {
    // Call the previously registered `on_closed` callback to notify the owner
    // that this remote has been fully closed and all resources have been
    // released.
    self->on_closed(self, self->on_closed_data);
  }
}

/**
 * @brief Handles the closing process of a remote.
 *
 * This function is called when resources associated with the remote (such
 * as the connection) have been closed. It checks if all resources are properly
 * released using `ten_remote_could_be_close()`. If the remote can be
 * closed, it proceeds with `ten_remote_do_close()`.
 *
 * Note: This function is part of the bottom-up notification process in
 * the remote closing sequence, similar to the connection closing process.
 *
 * @param self Pointer to the remote to be closed.
 */
static void ten_remote_on_close(ten_remote_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  if (!ten_remote_could_be_close(self)) {
    TEN_LOGI("[%s] Could not close alive remote.",
             ten_string_get_raw_str(&self->uri));
    return;
  }

  TEN_LOGD("[%s] Remote can be closed now.",
           ten_string_get_raw_str(&self->uri));

  ten_remote_do_close(self);
}

/**
 * @brief Callback function invoked when a connection associated with a remote
 * is closed.
 *
 * This function is registered as the callback for connection closure events.
 * When a connection closes, this function is called to handle the remote's
 * response to that event. Depending on the remote's current state:
 * - If the remote is already in CLOSING state, it continues the remote
 *   closure process (bottom-up notification chain).
 * - If the remote is in any other state, it initiates the remote
 *   closure process since the underlying connection has closed unexpectedly.
 *
 * @param connection The connection that has been closed (marked as unused but
 * kept for callback signature).
 */
void ten_remote_on_connection_closed(TEN_UNUSED ten_connection_t *connection,
                                     void *on_closed_data) {
  ten_remote_t *remote = (ten_remote_t *)on_closed_data;
  TEN_ASSERT(remote, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true), "Should not happen.");
  TEN_ASSERT(
      connection && remote->connection && remote->connection == connection,
      "Invalid argument.");

  if (remote->state == TEN_REMOTE_STATE_CLOSING) {
    // The remote is already in CLOSING state, which means the closure was
    // initiated by the TEN runtime (top-down closure chain), e.g.:
    // app => engine => remote => connection => protocol.
    // Now that the connection has closed (bottom-up notification), we can
    // continue with closing this remote and notify our parent in the chain.
    ten_remote_on_close(remote);
  } else {
    // The connection has closed unexpectedly (not as part of a top-down closure
    // chain). This could be due to network issues, protocol errors, or other
    // failures. We need to initiate the remote closure process to clean up
    // resources and notify the engine about this disconnection.
    ten_remote_close(remote);
  }
}

static ten_remote_t *ten_remote_create_empty(const char *uri,
                                             ten_connection_t *connection) {
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  ten_remote_t *self = (ten_remote_t *)TEN_MALLOC(sizeof(ten_remote_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_REMOTE_SIGNATURE);

  self->state = TEN_REMOTE_STATE_INIT;

  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->on_msg = NULL;
  self->on_msg_data = NULL;

  self->on_server_connected = NULL;
  self->on_server_connected_cmd = NULL;

  ten_string_init_formatted(&self->uri, "%s", uri ? uri : "");

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  ten_connection_attach_to_remote(connection, self);

  self->connection = connection;

  self->engine = NULL;

  return self;
}

static void ten_remote_set_on_closed(ten_remote_t *self,
                                     ten_remote_on_closed_func_t on_close,
                                     void *on_close_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  self->on_closed = on_close;
  self->on_closed_data = on_close_data;
}

static void ten_remote_set_on_msg(ten_remote_t *self,
                                  ten_remote_on_msg_func_t on_msg,
                                  void *on_msg_data) {
  TEN_ASSERT(self && ten_remote_check_integrity(self, true) && on_msg,
             "Should not happen.");

  self->on_msg = on_msg;
  self->on_msg_data = on_msg_data;
}

/**
 * @brief Attaches a remote instance to an engine.
 *
 * This function establishes a bidirectional relationship between a remote and
 * an engine.
 *
 * @param self Pointer to the remote instance to be attached.
 */
static void ten_remote_attach_to_engine(ten_remote_t *self,
                                        ten_engine_t *engine) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  self->engine = engine;

  // Register a callback for the remote to send messages to the engine. When the
  // remote receives a message, it will forward it to the engine through this
  // callback. The engine is passed implicitly as it's stored in the remote's
  // context.
  ten_remote_set_on_msg(self, ten_engine_receive_msg_from_remote, engine);

  // Register a callback to notify the engine when this remote is closed. This
  // allows the engine to clean up any resources associated with this remote and
  // update its internal state. The engine instance is passed as the callback
  // data.
  ten_remote_set_on_closed(self, ten_engine_on_remote_closed, engine);
}

ten_remote_t *ten_remote_create_for_engine(const char *uri,
                                           ten_engine_t *engine,
                                           ten_connection_t *connection) {
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  // NOTE: Whether the remote uri is duplicated in the engine should _not_ be
  // checked when the remote is created, but should be checked when the engine
  // is trying to connect to the remote.

  ten_remote_t *self = ten_remote_create_empty(uri, connection);
  ten_remote_attach_to_engine(self, engine);

  return self;
}

/**
 * @brief Closes a remote connection.
 *
 * This function initiates the closing process for a remote connection. If the
 * remote is already in the process of closing state, the function returns
 * without taking any action. Otherwise, it sets the closing state and proceeds
 * with the closing process.
 *
 * If the remote has an active connection that is not already closed, it will
 * close the connection, which will eventually trigger `ten_remote_on_close()`
 * when the connection is fully closed. If there is no connection or the
 * connection is already closed, `ten_remote_on_close()` is called immediately.
 *
 * @param self Pointer to the remote instance to close.
 */
void ten_remote_close(ten_remote_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  if (self->state == TEN_REMOTE_STATE_CLOSING) {
    return;
  }

  TEN_LOGD("Try to close remote (%s)", ten_string_get_raw_str(&self->uri));

  self->state = TEN_REMOTE_STATE_CLOSING;

  if (self->connection &&
      self->connection->state < TEN_CONNECTION_STATE_CLOSING) {
    ten_connection_close(self->connection);
  } else {
    // This remote could be closed directly.
    ten_remote_on_close(self);
  }
}

bool ten_remote_on_input(ten_remote_t *self, ten_shared_ptr_t *msg,
                         ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(self->engine && ten_engine_check_integrity(self->engine, true),
             "Should not happen.");

  if (self->on_msg) {
    // The source of all the messages coming from this remote will be
    // 'remote->uri'. Remote URI is used to identify the identity of the other
    // side, ex: the other side is a TEN app or a TEN client.
    ten_msg_set_src_uri(msg, ten_string_get_raw_str(&self->uri));

    return self->on_msg(self, msg, self->on_msg_data);
  }
  return true;
}

bool ten_remote_send_msg(ten_remote_t *self, ten_shared_ptr_t *msg,
                         ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  if (self->state == TEN_REMOTE_STATE_CLOSING) {
    // The remote is closing, do not proceed to send this message to
    // 'connection'.
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "Remote is closing, do not proceed to send this message.");
    }

    return false;
  }

  ten_connection_t *connection = self->connection;
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Access across threads.");
  TEN_ASSERT(connection->duplicate == false &&
                 ten_connection_attach_to(self->connection) ==
                     TEN_CONNECTION_ATTACH_TO_REMOTE,
             "Connection should attach to remote.");

  ten_connection_send_msg(connection, msg);

  return true;
}

/**
 * @brief Callback function invoked when a connection attempt to a remote server
 * completes.
 *
 * This function is called by the protocol layer when a connection attempt
 * to a remote server either succeeds or fails. If the connection is successful,
 * it invokes the remote's `on_server_connected` callback. If the connection
 * fails, it invokes the remote's `on_error` callback.
 *
 * The function follows the chain: protocol -> connection -> remote -> engine,
 * verifying the integrity of each component along the way.
 *
 * @param protocol The protocol instance that attempted the connection.
 * @param success Boolean indicating whether the connection attempt was
 * successful.
 */
static void on_server_connected(ten_protocol_t *protocol, bool success) {
  TEN_ASSERT(
      protocol && ten_protocol_check_integrity(protocol, true) &&
          ten_protocol_attach_to(protocol) == TEN_PROTOCOL_ATTACH_TO_CONNECTION,
      "Should not happen.");

  ten_connection_t *connection = protocol->attached_target.connection;
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true) &&
                 ten_connection_attach_to(connection) ==
                     TEN_CONNECTION_ATTACH_TO_REMOTE,
             "Should not happen.");

  ten_remote_t *remote = connection->attached_target.remote;
  TEN_ASSERT(remote && ten_remote_check_integrity(remote, true),
             "Should not happen.");

  TEN_ASSERT(remote->engine && ten_engine_check_integrity(remote->engine, true),
             "Should not happen.");

  if (success) {
    TEN_LOGD("Connected to remote (%s)", ten_string_get_raw_str(&remote->uri));

    if (remote->on_server_connected) {
      // Invoke the success callback with the stored command.
      remote->on_server_connected(remote, remote->on_server_connected_cmd);
    }
  } else {
    TEN_LOGW("Failed to connect to a remote (%s)",
             ten_string_get_raw_str(&remote->uri));

    if (remote->on_error) {
      // Invoke the error callback with the stored command.
      remote->on_error(remote, remote->on_server_connected_cmd);
    }
  }

  // Clear the callback after use to prevent double invocation.
  remote->on_server_connected = NULL;
  remote->on_error = NULL;
}

/**
 * @brief Initiates a connection to a remote server.
 *
 * This function attempts to establish a connection to a remote server using the
 * URI stored in the remote object. It sets up callbacks to be invoked when the
 * connection succeeds or fails.
 *
 * The function stores the provided callbacks and command in the remote object,
 * which will be used by the `on_server_connected` callback when the connection
 * attempt completes.
 *
 * @param self The remote instance initiating the connection. Must not be NULL.
 * @param connected Callback function to be invoked if the connection succeeds.
 * @param on_server_connected_cmd Command to be passed to the callbacks when
 * invoked. If not NULL, it will be cloned and stored in the remote object.
 * @param on_error Callback function to be invoked if the connection fails.
 */
void ten_remote_connect_to(ten_remote_t *self,
                           ten_remote_on_server_connected_func_t connected,
                           ten_shared_ptr_t *on_server_connected_cmd,
                           ten_remote_on_error_func_t on_error) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_remote_check_integrity(self, true), "Should not happen.");

  TEN_ASSERT(self->engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self->engine, true),
             "Should not happen.");

  self->on_server_connected = connected;

  TEN_ASSERT(!self->on_server_connected_cmd, "Should not happen.");
  if (on_server_connected_cmd) {
    self->on_server_connected_cmd =
        ten_shared_ptr_clone(on_server_connected_cmd);
  }

  self->on_error = on_error;

  ten_connection_connect_to(self->connection,
                            ten_string_get_raw_str(&self->uri),
                            on_server_connected);
}

bool ten_remote_is_uri_equal_to(ten_remote_t *self, const char *uri) {
  TEN_ASSERT(self && ten_remote_check_integrity(self, true) && uri,
             "Should not happen.");

  if (ten_string_is_equal_c_str(&self->uri, uri)) {
    return true;
  }

  return false;
}

ten_runloop_t *ten_remote_get_attached_runloop(ten_remote_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in
                 // different threads.
                 ten_remote_check_integrity(self, false),
             "Should not happen.");

  return ten_engine_get_attached_runloop(self->engine);
}
