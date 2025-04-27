//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/internal/close.h"

#include <stddef.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_runtime/timer/timer.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/macro/mark.h"

/**
 * @brief Synchronously closes the engine and its resources.
 *
 * This function initiates the closing process for the engine by stopping and
 * closing all engine resources (ex: timers, extension contexts, and remote
 * connections). If there are no resources to close and the engine is already in
 * closing state, it will trigger the on_close callback immediately.
 *
 * @param self Pointer to the engine instance to close.
 */
static void ten_engine_close_sync(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  TEN_LOGI("[%s] Start to close engine.", ten_engine_get_id(self, true));

  self->is_closing = true;

  bool nothing_to_do = true;

  // Iterate through all timers in the engine and stop/close them. This ensures
  // all timer resources are properly cleaned up during engine shutdown.
  ten_list_foreach (&self->timers, iter) {
    ten_timer_t *timer = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(timer && ten_timer_check_integrity(timer, true),
               "Should not happen.");

    ten_timer_stop_async(timer);
    ten_timer_close_async(timer);

    nothing_to_do = false;
  }

  if (self->extension_context) {
    // Close the extension context asynchronously.
    ten_extension_context_close(self->extension_context);

    nothing_to_do = false;
  }

  // Iterate through all remotes in the engine and close them. This ensures all
  // remote resources are properly cleaned up during engine shutdown.
  ten_hashtable_foreach(&self->remotes, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_remote_t *remote =
        CONTAINER_OF_FROM_OFFSET(hh, self->remotes.hh_offset);
    TEN_ASSERT(remote && ten_remote_check_integrity(remote, true),
               "Should not happen.");

    ten_remote_close(remote);

    nothing_to_do = false;
  }

  // Iterate through all weak remotes in the engine and close them. This ensures
  // all weak remote resources are properly cleaned up during engine shutdown.
  ten_list_foreach (&self->weak_remotes, iter) {
    ten_remote_t *remote = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(remote, "Invalid argument.");
    TEN_ASSERT(ten_remote_check_integrity(remote, true),
               "Invalid use of remote %p.", remote);

    ten_remote_close(remote);

    nothing_to_do = false;
  }

  if (self->has_uncompleted_async_task) {
    TEN_LOGI("[%s] Engine has an uncompleted async task, do not close.",
             ten_engine_get_id(self, true));

    nothing_to_do = false;
  }

  if (nothing_to_do) {
    ten_engine_on_close(self);
  }
}

/**
 * @brief Task function to close an engine asynchronously.
 *
 * This function is scheduled as a task on the engine's runloop to perform
 * the actual engine closing operation. It ensures the engine is closed
 * in a separate execution context from where the close was requested.
 *
 * @param engine_ Pointer to the engine to be closed (as void*).
 * @param arg Unused argument (required by task function signature).
 */
static void ten_engine_close_task(void *engine_, TEN_UNUSED void *arg) {
  ten_engine_t *engine = (ten_engine_t *)engine_;
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Invalid argument.");

  if (engine->is_closing) {
    TEN_LOGD("[%s] Engine is already closing, do not close again.",
             ten_engine_get_id(engine, true));
    goto done;
  }

  ten_engine_close_sync(engine);

done:
  ten_ref_dec_ref(&engine->ref);
}

/**
 * @brief Explanation of why engine closing should be asynchronous.
 *
 * Engine closing must always be performed asynchronously for safety reasons.
 * This ensures that code following the close request can still safely access
 * engine resources that haven't been destroyed yet.
 *
 * Problem with synchronous closing:
 * - When using 'ten_engine_close_sync()', resources like remotes with
 *   integrated protocols might be closed and destroyed immediately.
 * - Any code that executes after the close call might still try to access these
 *   now-destroyed resources, causing crashes or undefined behavior.
 *
 * While we could make individual resource closings (like remotes) asynchronous,
 * this approach is incomplete because:
 * - Other engine resources might still be accessed after the close call.
 * - A comprehensive solution requires making the entire engine closing
 *   operation asynchronous.
 *
 * Call stack comparison:
 *
 * Synchronous (unsafe):           Asynchronous (safe):
 * -------------------------       --------------------------
 * | caller function        |      | caller function        |
 * -------------------------       --------------------------
 * | other operations       | ---> | ten_engine_close_async | (schedules close)
 * |                        |      --------------------------
 * | ten_engine_close_sync  |      | continues execution    |
 * -------------------------       | safely                 |
 * | destroy resources      |      --------------------------
 * -------------------------
 *                                 Later, in a new call stack:
 *                                 --------------------------
 *                                 | ten_engine_close_task  |
 *                                 --------------------------
 *                                 | destroy resources      |
 *                                 --------------------------
 */
void ten_engine_close_async(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in different
      // threads.
      ten_engine_check_integrity(self, false), "Should not happen.");

  TEN_LOGI("[%s] Try to close engine.", ten_engine_get_id(self, false));

  ten_ref_inc_ref(&self->ref);

  int rc = ten_runloop_post_task_tail(ten_engine_get_attached_runloop(self),
                                      ten_engine_close_task, self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);
    ten_ref_dec_ref(&self->ref);
  }
}

static size_t ten_engine_unclosed_remotes_cnt(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  size_t unclosed_remotes = 0;

  ten_hashtable_foreach(&self->remotes, iter) {
    ten_hashhandle_t *hh = iter.node;
    ten_remote_t *remote =
        CONTAINER_OF_FROM_OFFSET(hh, self->remotes.hh_offset);
    TEN_ASSERT(remote && ten_remote_check_integrity(remote, true),
               "Should not happen.");

    if (remote->state != TEN_REMOTE_STATE_CLOSED) {
      unclosed_remotes++;
    }
  }

  ten_list_foreach (&self->weak_remotes, iter) {
    ten_remote_t *remote = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(remote, "Invalid argument.");
    TEN_ASSERT(ten_remote_check_integrity(remote, true),
               "Invalid use of remote %p.", remote);

    if (remote->state != TEN_REMOTE_STATE_CLOSED) {
      unclosed_remotes++;
    }
  }

  return unclosed_remotes;
}

static bool ten_engine_could_be_close(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  size_t unclosed_remotes = ten_engine_unclosed_remotes_cnt(self);

  if (unclosed_remotes == 0 && ten_list_is_empty(&self->timers) &&
      (self->extension_context == NULL) && !self->has_uncompleted_async_task) {
    return true;
  } else {
    TEN_LOGI(
        "[%s] Could not close engine with alive resources: %zu remotes, %zu "
        "timers, %p extension_context, %s",
        ten_engine_get_id(self, true), unclosed_remotes,
        ten_list_size(&self->timers), self->extension_context,
        self->has_uncompleted_async_task ? "has uncompleted async task"
                                         : "no uncompleted async task");

    return false;
  }
}

/**
 * @brief Performs the actual closing of the engine.
 *
 * This function handles the engine closure in two different ways depending on
 * whether the engine owns its runloop:
 * 1. If the engine has its own runloop: It stops the runloop, which will
 *    trigger the `on_closed` callback when the runloop fully stops (via the
 *    runloop's `on_stopped` mechanism).
 * 2. If the engine doesn't own the runloop: It directly calls the `on_closed`
 *    callback if one is registered.
 *
 * @param self The engine to close.
 */
static void ten_engine_do_close(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  if (self->has_own_loop) {
    // Stop the event loop belonging to this engine. The `on_closed` callback
    // will be called after the event loop has been stopped via the runloop's
    // `on_stopped` mechanism. The engine will be destroyed at that time as
    // well.
    ten_runloop_stop(self->loop);
  } else {
    // For engines that don't own their runloop, we need to directly call the
    // `on_closed` callback if one is registered.
    if (self->on_closed) {
      self->on_closed(self, self->on_closed_data);
    }
  }
}

void ten_engine_on_close(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  if (!ten_engine_could_be_close(self)) {
    TEN_LOGD("[%s] Could not close alive engine.",
             ten_engine_get_id(self, true));
    return;
  }

  TEN_LOGD("[%s] Engine can be closed now.", ten_engine_get_id(self, true));

  ten_engine_do_close(self);
}

void ten_engine_on_timer_closed(ten_timer_t *timer, void *on_closed_data) {
  TEN_ASSERT(timer, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(timer, true), "Should not happen.");

  ten_engine_t *engine = (ten_engine_t *)on_closed_data;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  // Remove the timer from the timer list.
  ten_list_remove_ptr(&engine->timers, timer);

  if (engine->is_closing) {
    ten_engine_on_close(engine);
  }
}

void ten_engine_on_extension_context_closed(
    ten_extension_context_t *extension_context, void *on_closed_data) {
  TEN_ASSERT(extension_context, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, true),
             "Invalid use of extension_context %p.", extension_context);

  ten_engine_t *engine = (ten_engine_t *)on_closed_data;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  TEN_LOGD("[%s] Extension context closed.", ten_engine_get_id(engine, true));

  // The extension context is closed, so set the pointer to NULL to prevent it
  // from being used again.
  engine->extension_context = NULL;

  if (engine->is_closing) {
    ten_engine_on_close(engine);
  }
}

void ten_engine_set_on_closed(ten_engine_t *self,
                              ten_engine_on_closed_func_t on_closed,
                              void *on_closed_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, false), "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Should not happen.");

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}
