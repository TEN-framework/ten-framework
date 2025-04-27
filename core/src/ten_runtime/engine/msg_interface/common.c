//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/msg_interface/common.h"

#include <stddef.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/remote_interface.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg_info.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void ten_engine_prepend_to_in_msgs_queue(ten_engine_t *self,
                                                ten_list_t *msgs) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  if (ten_list_size(msgs)) {
    TEN_UNUSED int rc = ten_mutex_lock(self->in_msgs_lock);
    TEN_ASSERT(!rc, "Should not happen.");

    ten_list_concat(msgs, &self->in_msgs);
    ten_list_swap(msgs, &self->in_msgs);

    rc = ten_mutex_unlock(self->in_msgs_lock);
    TEN_ASSERT(!rc, "Should not happen.");
  }
}

static void ten_engine_handle_msg(ten_engine_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  if (self->is_closing && !ten_msg_type_to_handle_when_closing(msg)) {
    // Except some special commands, do not handle messages anymore if the
    // engine is closing.
    return;
  }

  if (ten_msg_is_cmd_and_result(msg)) {
    // Because the command ID is a critical information which is necessary for
    // the correct handling of all command-type messages, we need to assign a
    // command ID to messages which don't have one.
    ten_cmd_base_gen_cmd_id_if_empty(msg);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_msg_engine_handler_func_t engine_handler =
      ten_msg_info[ten_msg_get_type(msg)].engine_handler;
  if (engine_handler) {
    engine_handler(self, msg, &err);
  }

  ten_error_deinit(&err);
}

static void ten_engine_handle_in_msgs_sync(ten_engine_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  ten_list_t in_msgs_ = TEN_LIST_INIT_VAL;

  TEN_UNUSED int rc = ten_mutex_lock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_list_swap(&in_msgs_, &self->in_msgs);

  rc = ten_mutex_unlock(self->in_msgs_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  // This list stores any msgs which needs to be put back to the in_msgs queue.
  ten_list_t put_back_msgs = TEN_LIST_INIT_VAL;

  ten_list_foreach (&in_msgs_, iter) {
    ten_shared_ptr_t *msg = ten_smart_ptr_listnode_get(iter.node);
    TEN_ASSERT(msg, "Should not happen.");
    TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");
    TEN_ASSERT(!ten_msg_src_is_empty(msg),
               "The message source should have been set.");

    if (ten_msg_is_cmd_and_result(msg)) {
      const char *src_uri = ten_msg_get_src_app_uri(msg);
      TEN_ASSERT(src_uri, "Should not happen.");

      ten_connection_t *orphan_connection =
          ten_engine_find_orphan_connection(self, src_uri);
      if (orphan_connection) {
        // If 'connection' is non-NULL, it means the command is from externally
        // (another external TEN app or client), so we need to check if the
        // 'connection' is duplicated.
        //
        // - If it is duplicated, remove it, and do not handle this command.
        // - Otherwise, create a 'remote' for this connection if there is none.

        // The connection should have already migrated to the engine thread, so
        // the thread safety of 'connection' can be maintained.
        TEN_ASSERT(ten_connection_check_integrity(orphan_connection, true),
                   "Should not happen.");
        TEN_ASSERT(
            ten_connection_get_migration_state(orphan_connection) ==
                TEN_CONNECTION_MIGRATION_STATE_DONE,
            "The connection migration must be completed before the engine "
            "handling the cmd.");

        // The 'start_graph' command should ensure that there is only one unique
        // channel between any two TEN apps in the graph.
        if ((ten_msg_get_type(msg) == TEN_MSG_TYPE_CMD_START_GRAPH) &&
            // Check if there is already a 'remote' for the other side.
            ten_engine_check_remote_is_duplicated(
                self, ten_msg_get_src_app_uri(msg))) {
          // Do not handle this 'start_graph' command, return a special
          // 'duplicate' result to the remote TEN app, so that it can close this
          // connection, and this TEN app could know that the closing of that
          // connection is normal (through the 'connect->duplicate' variable),
          // not an error condition, so does _not_ trigger the closing of the
          // whole engine.

          ten_connection_reply_result_for_duplicate_connection(
              orphan_connection, msg);

          // The cmd result goes to the other side directly, so do not route
          // 'duplicate' cmd result to engine.
          continue;
        } else {
          // If this connection doesn't attach to a remote, we need to create
          // a remote for this connection before the engine starting to
          // dispatch the message.
          ten_engine_link_orphan_connection_to_remote(
              self, orphan_connection, ten_msg_get_src_app_uri(msg));
        }
      }
    }

    if (ten_engine_is_ready_to_handle_msg(self)) {
      // Only trigger the engine to handle messages if it is ready.
      ten_engine_dispatch_msg(self, msg);
    } else {
      switch (ten_msg_get_type(msg)) {
      case TEN_MSG_TYPE_CMD_START_GRAPH:
      case TEN_MSG_TYPE_CMD_RESULT:
        // The only message types which can be handled before the engine is
        // ready is relevant to 'start_graph' command.
        ten_engine_handle_msg(self, msg);
        break;

      default:
        // Otherwise put back those messages to the original external commands
        // queue.
        //
        // ten_msg_dump(msg, NULL,
        //              "Engine is unable to handle msg now, put back it:
        //              ^m");
        ten_list_push_smart_ptr_back(&put_back_msgs, msg);
        break;
      }
    }
  }

  ten_list_clear(&in_msgs_);

  // The commands in 'put back' queue should be in the front of in_msgs queue,
  // so that they can be handled first next time.
  ten_engine_prepend_to_in_msgs_queue(self, &put_back_msgs);
}

/**
 * @brief Task handler for processing incoming messages in the engine's thread.
 *
 * This function is executed in the engine's thread context when posted to the
 * engine's runloop by `ten_engine_handle_in_msgs_async()`. It processes all
 * pending incoming messages by calling `ten_engine_handle_in_msgs_sync()` and
 * then decreases the engine's reference count that was increased before
 * posting this task.
 *
 * @param engine_ Pointer to the engine instance.
 * @param arg Unused argument.
 *
 * @note Thread-safety: This function must only be executed in the engine's
 * thread. It is not meant to be called directly but rather posted as a task to
 * the engine's runloop.
 */
static void ten_engine_handle_in_msgs_task(void *engine_,
                                           TEN_UNUSED void *arg) {
  ten_engine_t *engine = (ten_engine_t *)engine_;
  TEN_ASSERT(engine, "Invalid engine pointer");
  TEN_ASSERT(ten_engine_check_integrity(engine, true),
             "Engine integrity check failed or wrong thread access");

  TEN_LOGD("[%s] Handle incoming messages.", ten_engine_get_id(engine, true));

  ten_engine_handle_in_msgs_sync(engine);

  // Decrease reference count that was increased in
  // `ten_engine_handle_in_msgs_async`.
  ten_ref_dec_ref(&engine->ref);
}

/**
 * @brief Asynchronously handles incoming messages for the engine.
 *
 * This function posts a task to the engine's runloop to process incoming
 * messages. It is designed to be called from any thread, not just the engine's
 * thread. The function increases the reference count of the engine before
 * posting the task and the corresponding task handler
 * (ten_engine_handle_in_msgs_task) will decrease the reference count after
 * processing the messages.
 *
 * @param self Pointer to the engine instance.
 *
 * @note Thread-safety: This function is thread-safe and can be called from any
 * thread. The engine's reference count is properly managed to ensure the engine
 * isn't destroyed while the task is pending.
 */
void ten_engine_handle_in_msgs_async(ten_engine_t *self) {
  TEN_ASSERT(self, "Invalid engine pointer");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called from different
  // threads.
  TEN_ASSERT(ten_engine_check_integrity(self, false),
             "Invalid engine integrity");

  // Increase reference count to prevent the engine from being destroyed
  // while the task is pending in the runloop.
  ten_ref_inc_ref(&self->ref);

  int rc =
      ten_runloop_post_task_tail(ten_engine_get_attached_runloop(self),
                                 ten_engine_handle_in_msgs_task, self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);

    // Decrease reference count if posting the task failed.
    ten_ref_dec_ref(&self->ref);
  }
}

/**
 * @brief Appends a message to the engine's incoming message queue and triggers
 * asynchronous processing.
 *
 * This function safely adds a message to the engine's incoming message queue
 * and schedules it for processing. It is designed to be called from any thread,
 * not just the engine's thread, making it suitable for cross-thread
 * communication with the engine.
 *
 * @param self Pointer to the engine instance.
 * @param msg Pointer to the shared message to be appended to the queue.
 *            The message's ownership is transferred to the queue.
 *
 * @note Thread-safety: This function is thread-safe and can be called from any
 * thread. The engine's incoming message queue is protected by a mutex to ensure
 * thread-safe access.
 */
void ten_engine_append_to_in_msgs_queue(ten_engine_t *self,
                                        ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called from threads other
  // than the engine thread.
  TEN_ASSERT(ten_engine_check_integrity(self, false),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(msg, "Invalid message pointer");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid message integrity.");

  ten_mutex_lock(self->in_msgs_lock);
  ten_list_push_smart_ptr_back(&self->in_msgs, msg);
  ten_mutex_unlock(self->in_msgs_lock);

  ten_engine_handle_in_msgs_async(self);
}

static void ten_engine_post_msg_to_extension_thread(
    ten_engine_t *self, ten_extension_thread_t *extension_thread,
    ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "Invalid use of engine %p.", self);

  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
             "Invalid use of extension %p.", extension_thread);
  TEN_ASSERT(msg && (ten_msg_get_dest_cnt(msg) == 1),
             "When this function is executed, there should be only one "
             "destination remaining in the message's dest.");

  // This function would be called from threads other than the specified
  // extension thread. However, because the runloop relevant functions called in
  // this function have thread-safety protection of mutex in them, we do not
  // need to use any further locking mechanisms in this function to do any
  // protection.

  if (ten_runloop_task_queue_size(extension_thread->runloop) >=
      EXTENSION_THREAD_QUEUE_SIZE) {
    if (!ten_msg_is_cmd_and_result(msg)) {
      TEN_LOGW(
          "Discard a data-like message (%s) because extension thread input "
          "buffer is full.",
          ten_msg_get_name(msg));
      return;
    }
  }

  msg = ten_shared_ptr_clone(msg);

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  ten_msg_set_timestamp(msg, ten_current_time_us());
#endif

  int rc = ten_runloop_post_task_tail(extension_thread->runloop,
                                      ten_extension_thread_handle_in_msg_task,
                                      extension_thread, msg);

  // The extension thread might have already terminated. Therefore, even though
  // the extension thread instance still exists, attempting to enqueue tasks
  // into it will not succeed. It is necessary to account for this scenario to
  // prevent memory leaks.
  if (rc) {
    TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);

    if (ten_msg_is_cmd(msg)) {
      // Create a cmd result to inform the sender that the destination extension
      // has been terminated.
      ten_engine_create_cmd_result_and_dispatch(
          self, msg, TEN_STATUS_CODE_ERROR,
          "The destination extension has been terminated.");
    }

    ten_shared_ptr_destroy(msg);
  }
}

bool ten_engine_dispatch_msg(ten_engine_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");
  TEN_ASSERT(ten_msg_get_dest_cnt(msg) == 1,
             "When this function is executed, there should be only one "
             "destination remaining in the message's dest.");

  ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(msg);
  TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc),
             "Should not happen.");

  ten_app_t *app = self->app;
  TEN_ASSERT(app, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The engine might have its own thread, and it is different
  // from the app's thread. When the engine is still alive, the app must also be
  // alive. Furthermore, the app associated with the engine remains unchanged
  // throughout the engine's lifecycle, and the app fields accessed underneath
  // are constant once the app is initialized. Therefore, the use of the app
  // here is considered thread-safe.
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid use of app %p.",
             app);

  if (!ten_string_is_equal_c_str(&dest_loc->app_uri, ten_app_get_uri(app))) {
    TEN_ASSERT(!ten_string_is_empty(&dest_loc->app_uri),
               "The uri of the app should not be empty.");

    // The message is _not_ for the current TEN app, so route the message to the
    // correct TEN app through the correct remote.
    ten_engine_route_msg_to_remote(self, msg);
  } else {
    // The destination of the message is the current TEN app.

    if (
        // It means asking the current TEN app to do something.
        ten_string_is_empty(&dest_loc->graph_id) ||
        // It means asking another engine in the same app to do something.
        !ten_string_is_equal(&dest_loc->graph_id, &self->graph_id)) {
      // Both of these 2 cases will need the current TEN app to dispatch the
      // message, and the threads of the TEN app and the current TEN engine
      // might be different, so push the message to the command queue of the
      // current TEN app.
      ten_app_push_to_in_msgs_queue(app, msg);
    } else {
      if (ten_string_is_empty(&dest_loc->extension_group_name)) {
        // It means the destination is the current engine, so ask the current
        // engine to handle this message.
        ten_engine_handle_msg(self, msg);
      } else {
        // Find the correct extension thread to handle this message.

        if (self->extension_context) {
          bool found = false;

          ten_list_foreach (&self->extension_context->extension_threads, iter) {
            ten_extension_thread_t *extension_thread =
                ten_ptr_listnode_get(iter.node);
            TEN_ASSERT(
                extension_thread &&
                    // TEN_NOLINTNEXTLINE(thread-check)
                    // thread-check: We are in the engine thread, _not_ in the
                    // extension thread. However, before the engine is closed,
                    // the pointer of the extension group and the pointer of the
                    // extension thread will not be changed, and the closing of
                    // the entire engine must start from the engine, so the
                    // execution to this position means that the engine has not
                    // been closed, so there will be no thread safety issue.
                    ten_extension_thread_check_integrity(extension_thread,
                                                         false),
                "Should not happen.");

            ten_extension_group_t *extension_group =
                extension_thread->extension_group;

            if (ten_string_is_equal(&extension_group->name,
                                    &dest_loc->extension_group_name)) {
              // Find the correct extension thread, ask it to handle the
              // message.
              found = true;
              ten_engine_post_msg_to_extension_thread(self, extension_thread,
                                                      msg);
              break;
            }
          }

          if (!found) {
            // ten_msg_dump(msg, NULL,
            //              "Failed to find the destination extension thread for
            //              " "the message ^m");

            if (ten_msg_is_cmd(msg)) {
              ten_shared_ptr_t *cmd_result =
                  ten_extension_group_create_cmd_result_for_invalid_dest(
                      msg, &dest_loc->extension_group_name);

              ten_engine_dispatch_msg(self, cmd_result);

              ten_shared_ptr_destroy(cmd_result);
            } else {
              // For a non-cmd message, it should be directly dropped without
              // replying with `cmd_result`. This situation occurs when there
              // are multiple `extension_thread`s within an `engine`. If
              // `extension thread A` sends a non-cmd message to `extension
              // thread B`, and the message first needs to be transmitted to the
              // `engine`, by the time the `engine` processes this non-cmd
              // message, `extension thread B` may have already terminated.
            }
          }
        }
      }
    }
  }

  return true;
}

void ten_engine_create_cmd_result_and_dispatch(ten_engine_t *self,
                                               ten_shared_ptr_t *origin_cmd,
                                               TEN_STATUS_CODE status_code,
                                               const char *detail) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(origin_cmd && ten_msg_is_cmd(origin_cmd), "Invalid argument.");

  ten_shared_ptr_t *cmd_result =
      ten_cmd_result_create_from_cmd(status_code, origin_cmd);

  // The engine currently does not support returning streaming cmd_results, so
  // all cmd_results actively sent by the engine should be considered `final`
  // cmd_results.
  bool rc = ten_cmd_result_set_final(cmd_result, true, NULL);
  TEN_ASSERT(rc, "Should not happen.");

  if (detail) {
    ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                         ten_value_create_string(detail), NULL);
  }

  ten_engine_dispatch_msg(self, cmd_result);

  ten_shared_ptr_destroy(cmd_result);
}
