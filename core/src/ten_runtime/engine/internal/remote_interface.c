//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/internal/remote_interface.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/extension_interface.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/engine/msg_interface/start_graph.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static bool ten_engine_del_weak_remote(ten_engine_t *self,
                                       ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(remote, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  bool success = ten_list_remove_ptr(&self->weak_remotes, remote);

  TEN_LOGV("[%s] Delete remote %p from weak list: %s",
           ten_engine_get_id(self, true), remote,
           success ? "success." : "failed.");

  return success;
}

ten_remote_t *ten_engine_find_weak_remote(ten_engine_t *self, const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  ten_list_foreach (&self->weak_remotes, iter) {
    ten_remote_t *remote = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(remote, "Invalid argument.");
    TEN_ASSERT(ten_remote_check_integrity(remote, true),
               "Invalid use of remote %p.", remote);

    if (ten_string_is_equal_c_str(&remote->uri, uri)) {
      return remote;
    }
  }

  return NULL;
}

static size_t ten_engine_weak_remotes_cnt_in_specified_uri(ten_engine_t *self,
                                                           const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  size_t cnt = ten_list_find_ptr_cnt_custom(&self->weak_remotes, uri,
                                            ten_remote_is_uri_equal_to);

  TEN_LOGV("[%s] weak remote cnt for %s: %zu", ten_engine_get_id(self, true),
           uri, cnt);

  return cnt;
}

static ten_engine_on_protocol_created_ctx_t *
ten_engine_on_protocol_created_ctx_create(ten_engine_on_remote_created_cb_t cb,
                                          void *user_data) {
  ten_engine_on_protocol_created_ctx_t *self =
      (ten_engine_on_protocol_created_ctx_t *)TEN_MALLOC(
          sizeof(ten_engine_on_protocol_created_ctx_t));

  self->cb = cb;
  self->user_data = user_data;

  return self;
}

static void ten_engine_on_protocol_created_ctx_destroy(
    ten_engine_on_protocol_created_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_FREE(self);
}

/**
 * @brief Callback function invoked when a remote connection is closed.
 *
 * This function handles the cleanup process when a remote connection is closed.
 * It determines whether the remote is a weak reference or a normal remote,
 * and performs the appropriate cleanup actions:
 * - For weak remotes: Removes from the `weak_remotes` list and destroys the
 *   remote.
 * - For normal remotes: Removes from the `remotes` hashtable if found.
 *
 * @param remote The remote instance that has been closed.
 * @param on_closed_data Callback data, which should be a pointer to the engine
 * instance.
 */
void ten_engine_on_remote_closed(ten_remote_t *remote, void *on_closed_data) {
  TEN_ASSERT(remote && on_closed_data, "Should not happen.");

  ten_engine_t *self = (ten_engine_t *)on_closed_data;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  // Verify that there's at most one weak remote with this URI. This is
  // important for proper cleanup and preventing memory leaks.
  TEN_ASSERT(ten_engine_weak_remotes_cnt_in_specified_uri(
                 self, ten_string_get_raw_str(&remote->uri)) <= 1,
             "There should be at most 1 weak remote of the specified uri.");

  // First, try to remove the remote from the weak_remotes list.
  bool is_weak = ten_engine_del_weak_remote(self, remote);
  if (is_weak) {
    // If it was a weak remote, simply destroy it. Weak remotes are temporary
    // and their closure doesn't affect engine state.
    ten_remote_destroy(remote);
  } else {
    // If not a weak remote, check if it's in the normal remotes hashtable.
    bool found_in_remotes = false;

    // Look up the remote in the remotes hashtable by its URI.
    ten_hashhandle_t *strong_remote_hh = ten_hashtable_find_string(
        &self->remotes, ten_string_get_raw_str(&remote->uri));
    if (strong_remote_hh) {
      ten_remote_t *strong_remote = CONTAINER_OF_FROM_FIELD(
          strong_remote_hh, ten_remote_t, hh_in_remote_table);
      TEN_ASSERT(strong_remote, "Invalid argument.");
      TEN_ASSERT(ten_remote_check_integrity(strong_remote, true),
                 "Invalid use of remote %p.", strong_remote);

      if (strong_remote == remote) {
        // Found the exact remote instance in the remotes hashtable.
        found_in_remotes = true;

        // Remove it from the hashtable (this doesn't destroy the remote yet).
        ten_hashtable_del(&self->remotes, strong_remote_hh);
      } else {
        // A different remote instance with the same URI exists in the
        // hashtable. This can happen with duplicate remotes, where one is being
        // closed but another with the same URI remains active.
      }
    }

    if (!found_in_remotes) {
      TEN_LOGI("[%s] The remote %p is not found in the 'remotes' list.",
               ten_engine_get_id(self, true), remote);

      // If the remote wasn't in either list, just destroy it directly. This can
      // happen in edge cases during cleanup or error handling.
      ten_remote_destroy(remote);
      return;
    }
  }

  // Handle engine lifecycle based on remote closure.
  if (self->is_closing) {
    // If the engine is already in closing state, continue its closure process.
    // This remote's closure might be the last step needed before engine
    // shutdown.
    ten_engine_on_close(self);
  } else {
    if (!is_weak && !self->long_running_mode) {
      // In normal mode (not long-running), closing any non-weak remote triggers
      // engine shutdown. This is the default behavior for short-lived engine
      // instances.
      //
      // Note: If this behavior is not desired, this line can be commented out
      // to allow the engine to continue running after remote closures.
      ten_engine_close_async(self);
    }
  }
}

static void ten_engine_add_remote(ten_engine_t *self, ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(remote, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  TEN_LOGD("[%s] Add %s (%p) as remote.", ten_engine_get_id(self, true),
           ten_string_get_raw_str(&remote->uri), remote);

  ten_hashtable_add_string(&self->remotes, &remote->hh_in_remote_table,
                           ten_string_get_raw_str(&remote->uri),
                           ten_remote_destroy);
}

static void ten_engine_add_weak_remote(ten_engine_t *self,
                                       ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(remote, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  TEN_LOGD("[%s] Add %s (%p) as weak remote.", ten_engine_get_id(self, true),
           ten_string_get_raw_str(&remote->uri), remote);

  TEN_UNUSED ten_listnode_t *found = ten_list_find_ptr_custom(
      &self->weak_remotes, ten_string_get_raw_str(&remote->uri),
      ten_remote_is_uri_equal_to);
  TEN_ASSERT(!found, "There should be at most 1 weak remote of %s.",
             ten_string_get_raw_str(&remote->uri));

  // Do not set 'ten_remote_destroy' as the destroy function, because we might
  // _move_ a weak remote out of 'weak_remotes' list when we ensure it is not
  // duplicated.
  ten_list_push_ptr_back(&self->weak_remotes, remote, NULL);
}

void ten_engine_upgrade_weak_remote_to_normal_remote(ten_engine_t *self,
                                                     ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(remote, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  ten_engine_del_weak_remote(self, remote);
  ten_engine_add_remote(self, remote);
}

static ten_remote_t *ten_engine_find_remote(ten_engine_t *self,
                                            const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(uri, "Should not happen.");

  ten_hashhandle_t *hh = ten_hashtable_find_string(&self->remotes, uri);
  if (hh) {
    return CONTAINER_OF_FROM_FIELD(hh, ten_remote_t, hh_in_remote_table);
  }

  return NULL;
}

void ten_engine_link_orphan_connection_to_remote(
    ten_engine_t *self, ten_connection_t *orphan_connection, const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(orphan_connection, "Invalid argument.");
  TEN_ASSERT(ten_connection_check_integrity(orphan_connection, true),
             "Invalid use of engine %p.", orphan_connection);

  TEN_ASSERT(uri, "Invalid argument.");
  TEN_ASSERT(
      !ten_engine_find_remote(self, uri),
      "The relationship of remote and connection should be 1-1 mapping.");

  ten_remote_t *remote =
      ten_remote_create_for_engine(uri, self, orphan_connection);
  ten_engine_add_remote(self, remote);

  ten_engine_del_orphan_connection(self, orphan_connection);

  // Register the remote as the callback handler when the connection closes.
  // This ensures that when the connection is closed (either normally or due to
  // errors), the remote will be notified via
  // `ten_remote_on_connection_closed()` and can properly clean up its resources
  // and update its state.
  ten_connection_set_on_closed(orphan_connection,
                               ten_remote_on_connection_closed, remote);
}

static void ten_engine_on_protocol_closed(ten_protocol_t *protocol,
                                          void *on_closed_data) {
  TEN_ASSERT(protocol, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "Invalid use of protocol %p.", protocol);

  ten_engine_t *self = (ten_engine_t *)on_closed_data;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);
  TEN_ASSERT(self->is_closing, "Should not happen.");

  // The callback for closing the protocol has already been invoked, so the
  // engine's `has_uncompleted_async_task` has been completed. The mechanism
  // that was blocking the engine closing process can now be released.
  self->has_uncompleted_async_task = false;

  // This protocol was created by the engine, so the engine initially holds a
  // reference count to it. Since the engine will no longer use this protocol,
  // it decrements the reference count to indicate that the engine no longer
  // needs it.
  ten_ref_dec_ref(&protocol->ref);

  // Continue with the engine's original closing flow.
  ten_engine_on_close(self);
}

static void ten_engine_on_protocol_created(ten_env_t *ten_env,
                                           ten_protocol_t *protocol,
                                           void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_engine_t *self = ten_env_get_attached_engine(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_engine_on_protocol_created_ctx_t *ctx =
      (ten_engine_on_protocol_created_ctx_t *)cb_data;

  if (self->is_closing) {
    TEN_LOGW(
        "[%s] Protocol created, but skip to create remote due to engine "
        "is closing.",
        ten_engine_get_id(self, true));

    if (ctx->cb) {
      ctx->cb(self, NULL, ctx->user_data);
    }

    ten_engine_on_protocol_created_ctx_destroy(ctx);

    ten_protocol_set_on_closed(protocol, ten_engine_on_protocol_closed, self);
    ten_protocol_close(protocol);

    return;
  }

  self->has_uncompleted_async_task = false;

  ten_connection_t *connection = ten_connection_create(protocol);
  TEN_ASSERT(connection, "Should not happen.");

  ten_string_copy(&connection->uri, &protocol->uri);

  // This is in the 'connect_to' stage, the 'connection' already attaches to the
  // engine, no migration is needed.
  ten_connection_set_migration_state(connection,
                                     TEN_CONNECTION_MIGRATION_STATE_DONE);

  ten_remote_t *remote = ten_remote_create_for_engine(
      ten_string_get_raw_str(&protocol->uri), self, connection);
  TEN_ASSERT(remote, "Should not happen.");

  // Register the remote as the callback handler when the connection closes.
  // This ensures that when the connection is closed (either normally or due to
  // errors), the remote will be notified via
  // `ten_remote_on_connection_closed()` and can properly clean up its resources
  // and update its state. This is particularly important for connections
  // created during the 'connect_to' stage to maintain the proper lifecycle
  // management of the remote object.
  ten_connection_set_on_closed(connection, ten_remote_on_connection_closed,
                               remote);

  if (ctx->cb) {
    ctx->cb(self, remote, ctx->user_data);
  }

  ten_engine_on_protocol_created_ctx_destroy(ctx);
}

static bool ten_engine_create_remote_async(
    ten_engine_t *self, const char *uri,
    ten_engine_on_remote_created_cb_t on_remote_created_cb, void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);
  TEN_ASSERT(on_remote_created_cb, "Invalid argument.");
  TEN_ASSERT(uri, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_engine_on_protocol_created_ctx_t *ctx =
      ten_engine_on_protocol_created_ctx_create(on_remote_created_cb, cb_data);
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  bool success = ten_addon_create_protocol_with_uri(
      self->ten_env, uri, TEN_PROTOCOL_ROLE_OUT_DEFAULT,
      ten_engine_on_protocol_created, ctx, &err);
  if (success) {
    self->has_uncompleted_async_task = true;
  } else {
    TEN_LOGE("[%s] Failed to create protocol for %s. err: %s",
             ten_engine_get_id(self, true), uri, ten_error_message(&err));
  }

  if (!success) {
    ten_engine_on_protocol_created_ctx_destroy(ctx);
  }

  ten_error_deinit(&err);

  return success;
}

/**
 * @brief The remote is connected successfully, it's time to send out the
 * message which was going to be sent originally.
 */
static void ten_engine_on_graph_remote_connected(ten_remote_t *self,
                                                 ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(self, true),
             "Invalid use of remote %p.", self);

  TEN_ASSERT(self->connection && ten_connection_attach_to(self->connection) ==
                                     TEN_CONNECTION_ATTACH_TO_REMOTE,
             "Should not happen.");

  TEN_ASSERT(self->connection->protocol &&
                 ten_protocol_attach_to(self->connection->protocol) ==
                     TEN_PROTOCOL_ATTACH_TO_CONNECTION,
             "Should not happen.");

  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  ten_protocol_send_msg(self->connection->protocol, cmd);

  ten_shared_ptr_destroy(cmd);
  self->on_server_connected_cmd = NULL;
}

/**
 * @brief Handles error when connecting to a remote for graph operations.
 *
 * This function is called when a connection attempt to a remote server fails
 * during graph operations.
 *
 * @param self The remote instance that failed to connect.
 * @param start_graph_cmd_for_the_remote The start_graph command that was meant
 *        to be sent to this remote. This command will be responded to with an
 *        error and then destroyed.
 */
static void ten_engine_on_graph_remote_connect_error(
    ten_remote_t *self, ten_shared_ptr_t *start_graph_cmd_for_the_remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(self, true),
             "Invalid use of remote %p.", self);

  TEN_ASSERT(start_graph_cmd_for_the_remote, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(start_graph_cmd_for_the_remote),
             "Invalid argument.");

  ten_engine_t *engine = self->engine;
  TEN_ASSERT(engine, "Invalid use of engine %p.", engine);
  TEN_ASSERT(ten_engine_check_integrity(engine, true),
             "Invalid use of engine %p.", engine);

  ten_engine_return_error_for_cmd_start_graph(
      engine, start_graph_cmd_for_the_remote, "Failed to connect to %s",
      ten_string_get_raw_str(&self->uri));

  // Clean up resources to prevent memory leaks.
  ten_shared_ptr_destroy(start_graph_cmd_for_the_remote);
  self->on_server_connected_cmd = NULL;

  // Close the remote connection since it failed to establish.
  ten_remote_close(self);
}

static void ten_engine_connect_to_remote_after_remote_is_created(
    ten_engine_t *engine, ten_remote_t *remote, void *user_data) {
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Invalid argument.");

  ten_shared_ptr_t *start_graph_cmd_for_the_remote =
      (ten_shared_ptr_t *)user_data;
  TEN_ASSERT(start_graph_cmd_for_the_remote, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(start_graph_cmd_for_the_remote),
             "Invalid argument.");

  // A simple sanity check:
  // Before starting to connect to more apps in the whole start_graph process,
  // `original_start_graph_cmd_of_enabling_engine` must be set. Otherwise,
  // after the entire process is completed, there will be no way to determine
  // where to send the `cmd_result` of the `start_graph` command.
  ten_shared_ptr_t *origin_start_graph_cmd =
      engine->original_start_graph_cmd_of_enabling_engine;
  TEN_ASSERT(origin_start_graph_cmd, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(origin_start_graph_cmd),
             "Should not happen.");

  if (!remote) {
    // Failed to create the remote instance. Just respond to the start_graph
    // command specifically issued for this `remote` with a response to simulate
    // an ERROR response from the `remote`, allowing the `engine` to continue
    // its process. After the `engine` completes its entire start_graph flow, it
    // will then respond to the `origin_start_graph_cmd`.
    ten_engine_return_error_for_cmd_start_graph(
        engine, start_graph_cmd_for_the_remote, "Failed to create remote (%s)",
        ten_msg_get_first_dest_uri(start_graph_cmd_for_the_remote));

    ten_shared_ptr_destroy(start_graph_cmd_for_the_remote);

    return;
  }

  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  if (ten_engine_check_remote_is_duplicated(
          engine, ten_string_get_raw_str(&remote->uri))) {
    // Since the remote_t creation is asynchronous, the engine may have already
    // established a new connection to the remote side during the creation
    // process. If it is found that a connection is about to be duplicated, this
    // remote object can be directly destroyed as the physical connection
    // has not actually been established. Additionally, there is no need to send
    // the 'start_graph' command to this remote, as the graph must have already
    // been started on the remote side.
    TEN_LOGD("[%s] Destroy remote %p(%s) because it's duplicated.",
             ten_engine_get_id(engine, true), remote,
             ten_string_get_raw_str(&remote->uri));

    ten_remote_close(remote);

    // Just respond to the start_graph command specifically issued for this
    // `remote` with a response to simulate an OK response from the `remote`,
    // allowing the `engine` to continue its process. After the `engine`
    // completes its entire start_graph flow, it will then respond to the
    // `origin_start_graph_cmd`.
    ten_engine_return_ok_for_cmd_start_graph(engine,
                                             start_graph_cmd_for_the_remote);

    ten_shared_ptr_destroy(start_graph_cmd_for_the_remote);
    return;
  }

  // This physical connection might be duplicated with other physical
  // connections between this TEN app and the remote TEN app. This situation may
  // appear in a graph which contains loops.
  //
  //                   ------->
  //  ----> TEN app 1            TEN app 2 <-----
  //                   <-------
  //
  // Although no duplicate conditions were found in the above check, duplicates
  // may still occur later, as this is just a matter of timing. Therefore, we
  // will first place this `remote` into a weak list, indicating that it has not
  // yet undergone the full duplication checks, and to prevent handling any
  // messages from this physical connection. Once we confirm that there are no
  // duplicates, we will move this `remote` from the weak list to the official
  // remote list.
  ten_engine_add_weak_remote(engine, remote);

  ten_remote_connect_to(remote, ten_engine_on_graph_remote_connected,
                        start_graph_cmd_for_the_remote,
                        ten_engine_on_graph_remote_connect_error);

  ten_shared_ptr_destroy(start_graph_cmd_for_the_remote);
}

bool ten_engine_connect_to_graph_remote(ten_engine_t *self, const char *uri,
                                        ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);
  TEN_ASSERT(uri, "Invalid argument.");
  TEN_ASSERT(cmd && ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_START_GRAPH,
             "Should not happen.");

  TEN_LOGD("[%s] Trying to connect to %s inside graph.",
           ten_engine_get_id(self, true), uri);

  return ten_engine_create_remote_async(
      self, uri, ten_engine_connect_to_remote_after_remote_is_created, cmd);
}

void ten_engine_route_msg_to_remote(ten_engine_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(
      msg && ten_msg_check_integrity(msg) && ten_msg_get_dest_cnt(msg) == 1,
      "Should not happen.");

  const char *dest_uri = ten_msg_get_first_dest_uri(msg);
  ten_remote_t *remote = ten_engine_find_remote(self, dest_uri);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = false;

  if (remote) {
    success = ten_remote_send_msg(remote, msg, &err);
  } else {
    TEN_LOGW("[%s] Could not find suitable remote based on uri: %s",
             ten_engine_get_id(self, true), dest_uri);

    ten_error_set(&err, TEN_ERROR_CODE_GENERIC,
                  "Could not find suitable remote based on uri: %s", dest_uri);
  }

  // It's unnecessary to search weak remotes, because weak remotes are not
  // ready to transfer messages.

  if (!success) {
    // If the message is a cmd, we should create a cmdResult to notify the
    // sender that the cmd is not successfully sent.
    if (ten_msg_is_cmd(msg)) {
      ten_engine_create_cmd_result_and_dispatch(
          self, msg, TEN_STATUS_CODE_ERROR, ten_error_message(&err));
    }
  }

  ten_error_deinit(&err);
}

ten_remote_t *ten_engine_check_remote_is_existed(ten_engine_t *self,
                                                 const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(uri, "Should not happen.");

  ten_remote_t *remote = NULL;

  // 1. Check if the remote is in the 'remotes' list.
  ten_hashhandle_t *hh = ten_hashtable_find_string(&self->remotes, uri);
  if (hh) {
#if defined(_DEBUG)
    size_t weak_remote_cnt = ten_list_find_ptr_cnt_custom(
        &self->weak_remotes, uri, ten_remote_is_uri_equal_to);
    // A remote might appear in 'remote' and 'weak_remote' once when the graph
    // contains a loop. This is the case of 'duplicate' connection.
    TEN_ASSERT(weak_remote_cnt <= 1, "Invalid numbers of weak remotes");
#endif

    remote = CONTAINER_OF_FROM_FIELD(hh, ten_remote_t, hh_in_remote_table);
    TEN_ASSERT(remote, "Invalid argument.");
    TEN_ASSERT(ten_remote_check_integrity(remote, true),
               "Invalid use of remote %p.", remote);

    TEN_LOGD("[%s] remote %p for uri '%s' is found in 'remotes' list.",
             ten_engine_get_id(self, true), remote, uri);

    return remote;
  }

  // 2. Check if the remote is in the 'weak_remotes' list.
  ten_listnode_t *found = ten_list_find_ptr_custom(&self->weak_remotes, uri,
                                                   ten_remote_is_uri_equal_to);

  if (found) {
    remote = ten_ptr_listnode_get(found);
    TEN_ASSERT(remote, "Invalid argument.");
    TEN_ASSERT(ten_remote_check_integrity(remote, true),
               "Invalid use of remote %p.", remote);
  }

  TEN_LOGD("[%s] remote %p for uri '%s' is%s in 'weak_remotes' list.",
           ten_engine_get_id(self, true), remote, uri, remote ? "" : " not");

  return remote;
}

// This function is used to solve the connection duplication problem. If there
// are two physical connections between two TEN apps, the connection which
// connects a TEN app with a smaller URI to a TEN app with a larger URI would be
// kept, and the other connection would be dropped.
//
//                   ------->
//  ----> TEN app 1            TEN app 2 <----
//                   <-------
bool ten_engine_check_remote_is_duplicated(ten_engine_t *self,
                                           const char *uri) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(uri, "Should not happen.");

  ten_remote_t *remote = ten_engine_check_remote_is_existed(self, uri);
  if (remote) {
    TEN_LOGW("[%s] Found a remote %s (%p), checking duplication...",
             ten_engine_get_id(self, true), uri, remote);

    if (ten_c_string_is_equal_or_smaller(uri, ten_app_get_uri(self->app))) {
      TEN_LOGW("[%s] > Remote %s (%p) is smaller, this channel is duplicated.",
               ten_engine_get_id(self, true), uri, remote);
      return true;
    } else {
      TEN_LOGW("[%s] > Remote %s (%p) is larger, keep this channel.",
               ten_engine_get_id(self, true), uri, remote);
    }
  }

  return false;
}

bool ten_engine_check_remote_is_weak(ten_engine_t *self, ten_remote_t *remote) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(remote, "Invalid argument.");
  TEN_ASSERT(ten_remote_check_integrity(remote, true),
             "Invalid use of remote %p.", remote);

  ten_listnode_t *found = ten_list_find_ptr(&self->weak_remotes, remote);

  TEN_LOGD("[%s] remote %p is%s weak.", ten_engine_get_id(self, true), remote,
           found ? "" : " not");

  return found != NULL;
}

bool ten_engine_receive_msg_from_remote(ten_remote_t *remote,
                                        ten_shared_ptr_t *msg,
                                        TEN_UNUSED void *user_data) {
  TEN_ASSERT(remote && ten_remote_check_integrity(remote, true),
             "Should not happen.");

  ten_engine_t *engine = remote->engine;
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true),
             "Invalid use of engine %p.", engine);

  // Assign the current engine as the message _source_ if there is none, so
  // that if this message traverse to another graph, the result could find the
  // way home.
  ten_msg_set_src_engine_if_unspecified(msg, engine);

  // The default destination engine would be the engine where this remote
  // attached to, if the message doesn't specify one.
  ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
      msg, engine, &engine->app->predefined_graph_infos);

  switch (ten_msg_get_type(msg)) {
  case TEN_MSG_TYPE_CMD_START_GRAPH: {
    // The 'start_graph' command could only be handled once in a graph.
    // Therefore, if we receive a new 'start_graph' command after the graph
    // has been established, just ignore this 'start_graph' command.

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, msg);
    ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                         ten_value_create_string(
                             "Receive a start_graph cmd after graph is built."),
                         NULL);

    ten_connection_send_msg(remote->connection, cmd_result);

    ten_shared_ptr_destroy(cmd_result);
    break;
  }

  default:
    ten_engine_dispatch_msg(engine, msg);
    break;
  }

  return true;
}
