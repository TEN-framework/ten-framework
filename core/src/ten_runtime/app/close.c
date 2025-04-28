//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/close.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/endpoint.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

static bool ten_app_has_no_work(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (ten_list_is_empty(&self->engines) &&
      ten_list_is_empty(&self->orphan_connections)) {
    return true;
  }

  return false;
}

static bool ten_app_could_be_close(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (ten_app_has_no_work(self) && ten_app_is_endpoint_closed(self)) {
    return true;
  }

  return false;
}

static void ten_app_proceed_to_close(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (!ten_app_could_be_close(self)) {
    TEN_LOGD("[%s] Could not close alive app.", ten_app_get_uri(self));
    return;
  }
  TEN_LOGD("[%s] Close app.", ten_app_get_uri(self));

  ten_app_on_deinit(self);
}

/**
 * @brief Initiates the synchronous closing process for all app resources.
 *
 * This function implements the top-down closure chain by closing all resources
 * owned by the app.
 *
 * Each of these resources will begin their own closing process, which will
 * eventually trigger callbacks that notify the app when they're fully closed.
 * This creates a bottom-up notification chain that eventually allows the app
 * to complete its own closure when all resources are properly released.
 *
 * Note: Despite the "sync" in the name, this function only initiates the
 * closing process and returns immediately. The actual closing happens
 * asynchronously through the closure chain.
 *
 * @param self Pointer to the app instance to close.
 */
static void ten_app_close_sync(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_LOGD("[%s] Try to close app.", ten_app_get_uri(self));

  // Close all engines attached to this app.
  ten_list_foreach (&self->engines, iter) {
    ten_engine_close_async(ten_ptr_listnode_get(iter.node));
  }

  // Close any orphaned connections (not attached to engines).
  ten_list_foreach (&self->orphan_connections, iter) {
    ten_connection_close(ten_ptr_listnode_get(iter.node));
  }

  // Close the endpoint protocol if it exists.
  if (self->endpoint_protocol) {
    ten_protocol_close(self->endpoint_protocol);
  }
}

/**
 * @brief Task function that handles the actual app closing process.
 *
 * This function is scheduled to run in the app's thread by `ten_app_close()`.
 * It implements the app closing logic.
 *
 * @param app_ Pointer to the app instance (passed as void* for task API)
 * @param arg Unused argument (required by task API signature)
 */
static void ten_app_close_task(void *app_, TEN_UNUSED void *arg) {
  ten_app_t *app = (ten_app_t *)app_;
  TEN_ASSERT(app_, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app_, true), "Should not happen.");

  // Check if the app can be closed immediately (no active resources or
  // resources already in closed state). This can happen if the app failed
  // during creation (e.g., due to invalid properties) and not all resources
  // were created.
  if (ten_app_could_be_close(app)) {
    TEN_LOGD("[%s] App could be closed now.", ten_app_get_uri(app));
    ten_app_proceed_to_close(app);
    return;
  }

  // App has active resources that need to be closed first. This initiates the
  // top-down closing sequence for all app resources.
  ten_app_close_sync(app);
}

/**
 * @brief Initiates the asynchronous closing process for an application.
 *
 * The actual closing process happens asynchronously in the app's thread.
 * This function can be safely called from any thread as it uses mutex
 * protection.
 *
 * If the app is already in the process of closing (state >=
 * TEN_APP_STATE_CLOSING), this function will return without taking any
 * additional action.
 *
 * @param self Pointer to the application instance to close.
 * @param err Error parameter (currently unused, reserved for future use).
 * @return true if the close request was successfully initiated or if the app
 *         was already closing, false otherwise (though currently always returns
 *         true).
 */
bool ten_app_close(ten_app_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(
                 self,
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is designed to be called from
                 // any thread (not just the app thread), so the whole function
                 // is protected by a lock.
                 false),
             "Should not happen.");

  ten_mutex_lock(self->state_lock);

  if (self->state >= TEN_APP_STATE_CLOSING) {
    TEN_LOGD("[%s] App is closing, do not close again.", ten_app_get_uri(self));
    goto done;
  }

  TEN_LOGD("[%s] Try to close app.", ten_app_get_uri(self));

  // Mark the app as closing before scheduling the actual close task.
  self->state = TEN_APP_STATE_CLOSING;

  // Schedule the close task to run in the app's thread.
  int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(self),
                                      ten_app_close_task, self, NULL);
  TEN_ASSERT(!rc, "Should not happen.");

done:
  ten_mutex_unlock(self->state_lock);
  return true;
}

bool ten_app_is_closing(ten_app_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is used to be called in threads other than the
  // app thread, so the whole function is protected by a lock.
  TEN_ASSERT(self && ten_app_check_integrity(self, false),
             "Should not happen.");

  bool is_closing = false;

  ten_mutex_lock(self->state_lock);
  is_closing = self->state >= TEN_APP_STATE_CLOSING ? true : false;
  ten_mutex_unlock(self->state_lock);

  return is_closing;
}

void ten_app_check_termination_when_engine_closed(ten_app_t *self,
                                                  ten_engine_t *engine) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (engine->has_own_loop) {
    // Wait for the engine thread to be reclaimed. Because the engine thread
    // should have been terminated, this operation should be very fast.
    TEN_LOGD("[%s:%s] App waiting engine thread be reclaimed.",
             ten_app_get_uri(self), ten_engine_get_id(engine, false));

    TEN_UNUSED int rc = ten_thread_join(
        ten_sanitizer_thread_check_get_belonging_thread(&engine->thread_check),
        -1);
    TEN_ASSERT(!rc, "Should not happen.");

    TEN_LOGD(
        "[%s:%s] Engine thread is reclaimed, and after this point, modify "
        "fields of 'engine' is safe.",
        ten_app_get_uri(self), ten_engine_get_id(engine, false));
  }

  if (engine->cmd_stop_graph != NULL) {
    const char *src_graph_id = ten_msg_get_src_graph_id(engine->cmd_stop_graph);
    if (!ten_string_is_equal_c_str(&engine->graph_id, src_graph_id)) {
      // This engine is _not_ suicidal.

      ten_app_create_cmd_result_and_dispatch(self, engine->cmd_stop_graph,
                                             TEN_STATUS_CODE_OK,
                                             "close engine done");
    }
  }

  ten_app_del_engine(self, engine);

  // At this point, if the engine had its own thread, that thread has already
  // been reclaimed (joined). For engines without their own thread, they share
  // the app's thread. In either case, it's now safe to destroy the engine
  // object by decrementing its reference count.
  ten_ref_dec_ref(&engine->ref);

  if (self->long_running_mode) {
    TEN_LOGD("[%s] Don't close App due to it's in long running mode.",
             ten_app_get_uri(self));
  } else {
    // Here, we do not rely on whether the app has any remaining orphan
    // connections to decide whether to shut it down. This is because if a bad
    // client connects to the app but does not perform any subsequent actions,
    // such as failing to send a `start_graph` command, the orphan connection
    // will remain indefinitely. Consequently, if the decision to close the app
    // is based on whether there are orphan connections, the app may never be
    // able to shut down. If you want to prevent the app from terminating itself
    // simply because there are no engines at a certain moment, you can use the
    // app's `long_running_mode` mechanism. Once `long_running_mode` is enabled,
    // the app must be explicitly closed using the `close_app` command. This is
    // both normal and reasonable behavior.
    if (ten_list_is_empty(&self->engines)) {
      ten_app_close(self, NULL);
    }
  }

  if (ten_app_is_closing(self)) {
    ten_app_proceed_to_close(self);
  }
}

void ten_app_on_orphan_connection_closed(ten_connection_t *connection,
                                         TEN_UNUSED void *on_closed_data) {
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  ten_app_t *self = connection->attached_target.app;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_LOGD("[%s] Orphan connection %p closed", ten_app_get_uri(self),
           connection);

  ten_app_del_orphan_connection(self, connection);
  ten_connection_destroy(connection);

  // Check if the app is in the closing phase.
  if (ten_app_is_closing(self)) {
    TEN_LOGD("[%s] App is closing, check to see if it could proceed.",
             ten_app_get_uri(self));
    ten_app_proceed_to_close(self);
  } else {
    // If 'connection' is an orphan connection, it means the connection is not
    // attached to an engine, and the TEN app should _not_ be closed due to an
    // strange connection like this, otherwise, the TEN app will be very
    // fragile, anyone could simply connect to the TEN app, and close the app
    // through disconnection.
  }
}

void ten_app_on_protocol_closed(TEN_UNUSED ten_protocol_t *protocol,
                                void *on_closed_data) {
  ten_app_t *self = (ten_app_t *)on_closed_data;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (ten_app_is_closing(self)) {
    ten_app_proceed_to_close(self);
  }
}
