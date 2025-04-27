//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/msg_interface/common.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/app/metadata.h"
#include "include_internal/ten_runtime/app/msg_interface/start_graph.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/migration.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"

void ten_app_do_connection_migration_or_push_to_engine_queue(
    ten_connection_t *connection, ten_engine_t *engine, ten_shared_ptr_t *msg) {
  // The 'connection' maybe NULL if the msg comes from other engines.
  if (connection) {
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: This function is called in the app thread. If the
    // connection has been migrated, its belonging thread will be the engine's
    // thread, so we do not check thread integrity here.
    TEN_ASSERT(ten_connection_check_integrity(connection, false),
               "Invalid argument.");
  }

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: We are in the app thread, and all the uses of the engine in
  // this function would not cause thread safety issues.
  TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
             "This function is called in the app thread.");

  if (connection && ten_connection_needs_to_migrate(connection, engine)) {
    ten_connection_migrate(connection, engine, msg);
  } else {
    ten_engine_append_to_in_msgs_queue(engine, msg);
  }
}

/**
 * @brief Default handler for messages that don't have specific handlers.
 *
 * This function processes messages that don't match any of the specific message
 * types handled by dedicated functions. It primarily routes messages to the
 * appropriate engine based on the destination graph ID, or handles error cases
 * when the target engine cannot be found.
 *
 * @param self Pointer to the app instance.
 * @param connection Pointer to the connection that received the message, or
 * NULL if the message was generated internally.
 * @param msg Shared pointer to the message to be handled.
 * @param err Pointer to an error structure to store any errors that occur.
 *
 * @return true if the message was handled successfully, false otherwise.
 */
static bool ten_app_handle_msg_default_handler(ten_app_t *self,
                                               ten_connection_t *connection,
                                               ten_shared_ptr_t *msg,
                                               ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");
  TEN_ASSERT(ten_msg_get_dest_cnt(msg) == 1, "Should not happen.");

  bool result = true;
  ten_string_t *dest_graph_id = &ten_msg_get_first_dest_loc(msg)->graph_id;

  if (ten_string_is_empty(dest_graph_id)) {
    // This means the destination is the app itself, not a specific graph.
    // Currently, the app doesn't need to handle any other message types,
    // so we just return success.
    return true;
  }

  // Determine which engine the message should go to based on the destination
  // graph ID.
  ten_engine_t *dest_engine = ten_app_get_engine_by_graph_id(
      self, ten_string_get_raw_str(dest_graph_id));
  if (dest_engine) {
    // The target engine is found, forward the message to it.

    // Correct the 'graph_id' from prebuilt-graph-name to engine-graph-id.
    ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
        msg, dest_engine, &self->predefined_graph_infos);

    // Either migrate the connection to the engine or add the message to the
    // engine's queue.
    ten_app_do_connection_migration_or_push_to_engine_queue(connection,
                                                            dest_engine, msg);
  } else {
    if (ten_msg_get_type(msg) != TEN_MSG_TYPE_CMD_RESULT) {
      // Failed to find the engine, check if the requested engine is a
      // _singleton_ prebuilt-graph engine that can be started on demand.
      ten_predefined_graph_info_t *predefined_graph_info =
          ten_app_get_singleton_predefined_graph_info_by_name(
              self, ten_string_get_raw_str(dest_graph_id));

      if (predefined_graph_info) {
        if (!ten_app_start_predefined_graph(self, predefined_graph_info, err)) {
          // Log the error but don't assert, as this is a recoverable error.
          TEN_LOGD("Failed to start predefined graph: %s",
                   err ? ten_error_message(err) : "unknown error");
          return false;
        }

        dest_engine = predefined_graph_info->engine;
        TEN_ASSERT(
            dest_engine && ten_engine_check_integrity(dest_engine, false),
            "Engine should be valid after starting predefined graph");
      }

      if (dest_engine) {
        // The target engine was successfully created, forward the message to
        // it.

        // Correct the 'graph_id' from prebuilt-graph-name to engine-graph-id.
        ten_msg_set_dest_engine_if_unspecified_or_predefined_graph_name(
            msg, dest_engine, &self->predefined_graph_infos);

        // Either migrate the connection to the engine or add the message to the
        // engine's queue.
        ten_app_do_connection_migration_or_push_to_engine_queue(
            connection, dest_engine, msg);
      } else {
        // Could not find or create the engine, return an error message.

        ten_shared_ptr_t *cmd_result =
            ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, msg);
        ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                             ten_value_create_string("Graph not found."), NULL);
        ten_msg_clear_and_set_dest_from_msg_src(cmd_result, msg);

        if (connection) {
          // The 'connection' is not NULL, which means that a message was sent
          // from the client side through an implementation protocol (ex:
          // msgpack or http). The implementation protocol only transfers one
          // message to the app even though it receives more than one message
          // at once, as the connection might need to be migrated and the
          // migration must happen only once. So all the events (ex: the closing
          // event, and other messages) of the implementation protocol will be
          // frozen before the migration is completed or reset.
          //
          // We could not find the engine for this message here, which means
          // this message is the first received by the 'connection', in other
          // words, the connection hasn't started doing migration yet. So we
          // have to reset the migration state, but not mark it as 'DONE', and
          // unfreeze the implementation protocol as it might have some pending
          // tasks (ex: the client disconnects, the implementation protocol
          // needs to be closed).

          // Important: These two operations must be performed in this order:
          // 1. Reset the migration state.
          // 2. Send the response message.
          //
          // Those two functions perform the following two actions:
          //
          // - ten_connection_migration_state_reset_when_engine_not_found()
          //   Sends a 'on_cleaned' event to the implementation protocol.
          //
          // - ten_connection_send_msg()
          //   Sends the result to the implementation protocol and then the
          //   result will be sent to the client.
          //
          // Suppose that the client sends a command to the app and closes the
          // app once it receives the cmd_result. Ex:
          //
          //   auto result = client.send_request_and_get_result();
          //   if (result) {
          //      app.close();
          //   }
          //
          // The closure of the app will send a 'close' event to the
          // implementation protocol. If those two functions are called
          // in reverse order, the execution sequence might be as follows:
          //
          //    [ client ]               [ app ]                [ protocol ]
          //     send cmd
          //                      ten_connection_send_msg()
          //                                               send cmd_result to
          //                                               client
          //
          //   close app
          //                                                 receive 'close'
          //                                                 event
          //                        reset_migration()
          //                                              receive 'on_cleaned'
          //                                              event
          //
          // We expect the implementation protocol to receive the 'on_cleaned'
          // event before the 'close' event, otherwise the 'close' event will be
          // frozen as the implementation protocol determines that the migration
          // is not completed yet.
          ten_connection_migration_state_reset_when_engine_not_found(
              connection);

          // Since this is an incorrect command (sent to a non-existent engine),
          // the migration was unsuccessful. Therefore, the connection's URI is
          // reset so that the source URI of the next command can potentially
          // become the URI of this connection.
          ten_string_clear(&connection->uri);

          // Send the error response back to the client.
          ten_connection_send_msg(connection, cmd_result);
        } else {
          // No connection means this message likely came from an extension in
          // another engine. Pass the error response back to the app for routing
          // back to the source.
          //
          // The 'msg' might be sent from extension A in engine 1 to extension B
          // in engine 2, there is no 'connection' in this case, the cmd result
          // should be sent back to engine 1.
          //
          // So, this cmd result needs to be passed back to the app for further
          // processing.
          result = ten_app_handle_in_msg(self, NULL, cmd_result, err);
        }

        ten_shared_ptr_destroy(cmd_result);
      }
    } else {
      // Just discard the cmd_result as its destination is missing. This is
      // acceptable because cmd_results are one-way messages and don't require
      // acknowledgment.
    }
  }

  return result;
}

static bool ten_app_handle_close_app_cmd(ten_app_t *self,
                                         ten_connection_t *connection,
                                         ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (connection) {
    TEN_ASSERT(ten_connection_check_integrity(connection, true),
               "Access across threads.");

    // This is the close_app command, so we do _not_ need to do any migration
    // tasks even if it should be done originally. We can declare that the
    // connection has already be migrated directly.
    ten_connection_upgrade_migration_state_to_done(connection, NULL);
  }

  TEN_LOGI("[%s] Close app due to 'close_app' cmd.", ten_app_get_uri(self));

  ten_app_close(self, err);

  return true;
}

static bool ten_app_handle_stop_graph_cmd(ten_app_t *self,
                                          ten_shared_ptr_t *cmd,
                                          TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");
  TEN_ASSERT(ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");
  TEN_ASSERT(ten_msg_get_dest_cnt(cmd) == 1, "Should not happen.");

  const char *dest_graph_id = ten_cmd_stop_graph_get_graph_id(cmd);
  // If the app needs to handle the `stop_graph` command, it means the app must
  // know the target's graph ID.
  TEN_ASSERT(strlen(dest_graph_id), "Should not happen.");

  ten_engine_t *dest_engine = NULL;

  // Find the engine based on the 'dest_graph_id' in the 'cmd'.
  ten_list_foreach (&self->engines, iter) {
    ten_engine_t *engine = ten_ptr_listnode_get(iter.node);

    if (ten_string_is_equal_c_str(&engine->graph_id, dest_graph_id)) {
      dest_engine = engine;
      break;
    }
  }

  if (dest_engine == NULL) {
    // Failed to find the engine by graph_id, send back an error message.
    ten_app_create_cmd_result_and_dispatch(
        self, cmd, TEN_STATUS_CODE_ERROR,
        "Failed to find the engine to be shut down.");

    return true;
  }

  // The engine is found, set the graph_id to the dest loc and send the 'cmd'
  // to the engine.
  ten_list_foreach (ten_msg_get_dest(cmd), iter) {
    ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc),
               "Should not happen.");

    ten_string_set_formatted(&dest_loc->graph_id, "%s",
                             ten_string_get_raw_str(&dest_engine->graph_id));
  }

  ten_engine_append_to_in_msgs_queue(dest_engine, cmd);

  return true;
}

/**
 * @brief Handles a cmd_result received by the app.
 *
 * @param self The app instance.
 * @param cmd_result The cmd_result to handle.
 * @param err Error information.
 *
 * @return true Always returns true to indicate the message was handled.
 */
static bool ten_app_handle_cmd_result(ten_app_t *self,
                                      ten_shared_ptr_t *cmd_result,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd_result, "Should not happen.");
  TEN_ASSERT(ten_cmd_base_check_integrity(cmd_result), "Should not happen.");
  TEN_ASSERT(ten_msg_get_type(cmd_result) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");
  TEN_ASSERT(ten_msg_get_dest_cnt(cmd_result) == 1, "Should not happen.");

  bool delete_msg = false;
  ten_shared_ptr_t *processed_cmd_result = NULL;

#if defined(_DEBUG)
  ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(cmd_result);
  TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc),
             "Should not happen.");

  ten_string_t loc_str;
  TEN_STRING_INIT(loc_str);
  ten_loc_to_string(dest_loc, &loc_str);
  TEN_LOGI("dest_loc: %s", ten_string_get_raw_str(&loc_str));
  ten_string_deinit(&loc_str);
#endif

  // Process the command result through the path table to handle multi-step
  // operations.
  bool proceed = ten_path_table_process_cmd_result(
      self->path_table, TEN_PATH_OUT, cmd_result, &processed_cmd_result);
  if (!proceed) {
    // The path is not complete yet (e.g., during 'start_graph' flow).
    TEN_LOGD(
        "The 'start_graph' flow is not completed, skip the cmd_result now.");
    return true;
  }

  // If path table returned a different message, use it and mark original for
  // deletion.
  if (cmd_result != processed_cmd_result) {
    cmd_result = processed_cmd_result;
    delete_msg = true;
  }

  // Extract the raw command result to access its handler.
  ten_cmd_base_t *raw_cmd_result = ten_cmd_base_get_raw_cmd_base(cmd_result);

  // Call the registered result handler if one exists.
  ten_env_transfer_msg_result_handler_func_t result_handler =
      ten_raw_cmd_base_get_result_handler(raw_cmd_result);
  if (result_handler) {
    result_handler(self->ten_env, cmd_result,
                   ten_raw_cmd_base_get_result_handler_data(raw_cmd_result),
                   NULL);
  }

  // Clean up the processed message if needed.
  if (delete_msg) {
    ten_shared_ptr_destroy(cmd_result);
  }

  return true;
}

bool ten_app_dispatch_msg(ten_app_t *self, ten_shared_ptr_t *msg,
                          ten_error_t *err) {
  ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(msg);
  TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc) &&
                 ten_msg_get_dest_cnt(msg) == 1,
             "Should not happen.");
  TEN_ASSERT(!ten_string_is_empty(&dest_loc->app_uri),
             "App URI should not be empty.");

  if (!ten_string_is_equal_c_str(&dest_loc->app_uri, ten_app_get_uri(self))) {
    TEN_ASSERT(0, "Handle this condition, msg dest '%s', app '%s'",
               ten_string_get_raw_str(&dest_loc->app_uri),
               ten_app_get_uri(self));
  } else {
    if (ten_string_is_empty(&dest_loc->graph_id)) {
      // It means asking the app to do something.

      ten_app_handle_in_msg(self, NULL, msg, err);
      ten_shared_ptr_destroy(msg);
    } else {
      TEN_ASSERT(0, "Handle this condition.");
    }
  }

  return true;
}

/**
 * @brief Determines if a message is destined for this application.
 *
 * This function checks if the destination location in a message refers to
 * this application. A message is considered destined for the application if:
 * 1. The app_uri in the destination matches this application's URI.
 * 2. The graph_id in the destination is empty (indicating the message is for
 *    the app itself, not for a specific graph within the app).
 *
 * @param self Pointer to the application instance.
 * @param dest_loc Pointer to the destination location structure from the
 * message.
 * @return true if the message is destined for this application, false
 * otherwise.
 *
 * @note This function performs integrity checks on both parameters to ensure
 *       they are valid before proceeding with the comparison.
 */
static bool ten_app_is_msg_dest(ten_app_t *self, ten_loc_t *dest_loc) {
  TEN_ASSERT(self, "Invalid app pointer.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Corrupted app structure.");

  TEN_ASSERT(dest_loc, "Invalid destination location pointer.");
  TEN_ASSERT(ten_loc_check_integrity(dest_loc),
             "Corrupted destination location structure.");

  return ten_string_is_equal_c_str(&dest_loc->app_uri, ten_app_get_uri(self)) &&
         ten_string_is_empty(&dest_loc->graph_id);
}

/**
 * @brief Handles an incoming message for the app.
 *
 * This function processes incoming messages based on their type and routes them
 * to the appropriate handler. It supports various message types including
 * start/stop graph commands, close app commands, and command results.
 *
 * @param self Pointer to the app instance.
 * @param connection Pointer to the connection that received the message, or
 * NULL if the message was generated internally.
 * @param msg Shared pointer to the message to be handled.
 * @param err Pointer to an error structure to store any errors that occur.
 *
 * @return true if the message was handled successfully, false otherwise.
 *
 * @note If a connection is provided, this function ensures that the connection
 *       is in a valid state for message processing, particularly during
 *       migration. Only messages in FIRST_MSG or DONE migration states are
 *       allowed.
 *
 * @note For cmd_result, the function first checks if this app is the intended
 * destination before handling it. If not, it falls through to the default
 * handler.
 */
bool ten_app_handle_in_msg(ten_app_t *self, ten_connection_t *connection,
                           ten_shared_ptr_t *msg, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid app pointer.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Corrupted app structure.");

  TEN_ASSERT(msg, "Invalid message pointer.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Corrupted message structure.");

  if (connection) {
    // If there is a 'connection', then it's possible that the connection might
    // need to be migrated, and if the connection is in the migration phase, we
    // can _not_ send the new messages to app. Therefore, we will control the
    // messages flow, to ensure that there will be only one message sent to the
    // app before the migration is completed.
    TEN_ASSERT(ten_connection_check_integrity(connection, true),
               "Access across threads.");

    TEN_CONNECTION_MIGRATION_STATE migration_state =
        ten_connection_get_migration_state(connection);
    TEN_ASSERT(migration_state == TEN_CONNECTION_MIGRATION_STATE_FIRST_MSG ||
                   migration_state == TEN_CONNECTION_MIGRATION_STATE_DONE,
               "Connection in invalid migration state.");
  }

  switch (ten_msg_get_type(msg)) {
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    return ten_app_handle_start_graph_cmd(self, connection, msg, err);

  case TEN_MSG_TYPE_CMD_CLOSE_APP:
    return ten_app_handle_close_app_cmd(self, connection, err);

  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
    return ten_app_handle_stop_graph_cmd(self, msg, err);

  case TEN_MSG_TYPE_CMD_RESULT:
    // Check if this app is the intended destination for the cmd_result.
    if (ten_app_is_msg_dest(self, ten_msg_get_first_dest_loc(msg))) {
      return ten_app_handle_cmd_result(self, msg, err);
    }
    // Fall through to default handler if not for this app.

  default:
    return ten_app_handle_msg_default_handler(self, connection, msg, err);
  }
}

static void ten_app_handle_in_msgs_sync(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_list_t in_msgs_ = TEN_LIST_INIT_VAL;

  TEN_UNUSED int rc = ten_mutex_lock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_list_swap(&in_msgs_, &self->in_msgs);

  rc = ten_mutex_unlock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_list_foreach (&in_msgs_, iter) {
    ten_shared_ptr_t *msg = ten_smart_ptr_listnode_get(iter.node);
    TEN_ASSERT(msg && ten_msg_check_integrity(msg) &&
                   !ten_msg_src_is_empty(msg) &&
                   (ten_msg_get_dest_cnt(msg) == 1),
               "Invalid argument.");

    // The 'ten_app_on_external_cmds()' is called in the following two scenarios
    // now.
    //
    // - Some cmds are sent from the extensions in the engine, and the receiver
    //   is the app, ex: the 'close_app' cmd.
    //
    // - Some cmds are sent from one engine, and the receiver is another engine
    //   in the app. The value of the cmd's 'origin_connection' field might or
    //   might not be NULL in this case.
    //
    //   * If the 'cmd' is sent from the extension, the 'origin_connection' is
    //     NULL. Ex: send the following cmd from extension in engine whose
    //     'graph_id' is A.
    //
    //   * If the 'cmd' is sent to another engine (whose 'graph_id' is B) from
    //     the client side, after the physical connection between the client and
    //     current engine (whose 'graph_id' is A) has been established, the
    //     'origin_connection' is not NULL. Ex: after the client sends the
    //     'start_graph' cmd to the engine whose 'graph_id' is A with the
    //     msgpack protocol, send a cmd to another engine, then the cmd will be
    //     received by engine A firstly.
    //
    //     The 'origin_connection' must belong to the remote in the engine who
    //     receives the cmd from the client side, no matter whether current
    //     engine is the expect receiver or not. Ex: the belonging of the
    //     'origin_connection' in this case is the remote in the engine A.
    //
    // Briefly, the above two scenarios are expected to handle the cmd out of
    // the scope of the engine which the cmd's 'origin_connection' belongs to.
    // And the following function -- 'ten_app_on_msg()' will do connection
    // migration if needed. So the cmd's 'origin_connection' _must_ _not_ be
    // passed to the 'ten_app_on_msg()' function here.
    ten_app_handle_in_msg(self, NULL, msg, &err);
  }

  ten_list_clear(&in_msgs_);

  ten_error_deinit(&err);
}

static void ten_app_handle_in_msgs_task(void *app_, TEN_UNUSED void *arg) {
  ten_app_t *app = (ten_app_t *)app_;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_app_handle_in_msgs_sync(app);
}

static void ten_app_handle_in_msgs_async(ten_app_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called outside
                 // of the TEN app thread.
                 ten_app_check_integrity(self, false),
             "Should not happen.");

  int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(self),
                                      ten_app_handle_in_msgs_task, self, NULL);
  TEN_ASSERT(!rc, "Should not happen.");
}

void ten_app_push_to_in_msgs_queue(ten_app_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self && ten_app_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(msg && ten_msg_is_cmd_and_result(msg), "Invalid argument.");
  TEN_ASSERT(!ten_cmd_base_cmd_id_is_empty(msg), "Invalid argument.");
  TEN_ASSERT(
      ten_msg_get_src_app_uri(msg) && strlen(ten_msg_get_src_app_uri(msg)),
      "Invalid argument.");
  TEN_ASSERT((ten_msg_get_dest_cnt(msg) == 1), "Invalid argument.");

  TEN_UNUSED bool rc = ten_mutex_lock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Failed to lock.");

  ten_list_push_smart_ptr_back(&self->in_msgs, msg);

  rc = ten_mutex_unlock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

  ten_app_handle_in_msgs_async(self);
}

void ten_app_create_cmd_result_and_dispatch(ten_app_t *self,
                                            ten_shared_ptr_t *origin_cmd,
                                            TEN_STATUS_CODE status_code,
                                            const char *detail) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(origin_cmd && ten_msg_is_cmd(origin_cmd), "Invalid argument.");

  ten_shared_ptr_t *cmd_result =
      ten_cmd_result_create_from_cmd(status_code, origin_cmd);

  if (detail) {
    ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                         ten_value_create_string(detail), NULL);
  }

  ten_app_push_to_in_msgs_queue(self, cmd_result);

  ten_shared_ptr_destroy(cmd_result);
}
