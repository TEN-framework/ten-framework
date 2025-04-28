//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/extension_interface.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/engine/msg_interface/start_graph.h"
#include "include_internal/ten_runtime/engine/on_xxx.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/extension_store/extension_store.h"
#include "include_internal/ten_runtime/extension_thread/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/sanitizer/thread_check.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_extension_thread_check_integrity_if_in_lock_mode(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (self->in_lock_mode) {
    return true;
  }

  return false;
}

bool ten_extension_thread_check_integrity(ten_extension_thread_t *self,
                                          bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_THREAD_SIGNATURE) {
    TEN_ASSERT(0,
               "Failed to pass extension_thread signature checking: %" PRId64,
               self->signature);
    return false;
  }

  if (check_thread) {
    if (ten_extension_thread_check_integrity_if_in_lock_mode(self)) {
      return true;
    }

    if (!ten_sanitizer_thread_check_do_check(&self->thread_check)) {
      TEN_ASSERT(0, "Failed to pass extension_thread thread safety checking.");
      return false;
    }
  }

  return true;
}

ten_extension_thread_t *ten_extension_thread_create(void) {
  ten_extension_thread_t *self =
      (ten_extension_thread_t *)TEN_MALLOC(sizeof(ten_extension_thread_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_EXTENSION_THREAD_SIGNATURE);

  self->state = TEN_EXTENSION_THREAD_STATE_INIT;
  self->is_close_triggered = false;

  self->extension_context = NULL;
  self->extension_group = NULL;
  self->extension_store = ten_extension_store_create(
      offsetof(ten_extension_t, hh_in_extension_store));

  ten_list_init(&self->extensions);
  self->extensions_cnt_of_deleted = 0;

  ten_list_init(&self->pending_msgs_received_in_init_stage);

  self->in_lock_mode = false;
  self->lock_mode_lock = ten_mutex_create();

  ten_sanitizer_thread_check_init(&self->thread_check);

  self->runloop = NULL;
  self->runloop_is_ready_to_use = ten_event_create(0, 0);

  return self;
}

static void ten_extension_thread_attach_to_group(
    ten_extension_thread_t *self, ten_extension_group_t *extension_group) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, false),
             "Invalid use of extension_thread %p.", self);
  TEN_ASSERT(extension_group, "Should not happen.");

  self->extension_group = extension_group;
}

void ten_extension_thread_attach_to_context_and_group(
    ten_extension_thread_t *self, ten_extension_context_t *extension_context,
    ten_extension_group_t *extension_group) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, false),
             "Invalid use of extension_thread %p.", self);
  TEN_ASSERT(extension_context && extension_group, "Should not happen.");

  self->extension_context = extension_context;
  ten_extension_thread_attach_to_group(self, extension_group);
}

void ten_extension_thread_destroy(ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, false),
             "Invalid use of extension_thread %p.", self);

  // All the Extensions should have been destroyed.
  TEN_ASSERT(ten_list_is_empty(&self->extensions), "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_list_clear(&self->pending_msgs_received_in_init_stage);

  if (self->runloop) {
    ten_runloop_destroy(self->runloop);
    self->runloop = NULL;
  }

  ten_event_destroy(self->runloop_is_ready_to_use);

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_extension_store_destroy(self->extension_store);

  ten_mutex_destroy(self->lock_mode_lock);
  self->lock_mode_lock = NULL;

  TEN_FREE(self);
}

// Notify extension context (engine) that we (extension thread) are closed, so
// that engine can join this thread to prevent memory leak.
static void ten_extension_thread_notify_engine_we_are_closed(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In the closing flow, the closing of the engine is always
  // after the closing of the extension thread, so its thread safe to access the
  // extension_context here.
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, false),
             "Invalid use of extension_context %p.", extension_context);

  ten_engine_t *engine = extension_context->engine;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In the closing flow, the closing of the engine is always
  // after the closing of the extension thread, so its thread safe to access the
  // runloop of the engine here.
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_runloop_t *engine_loop = ten_engine_get_attached_runloop(engine);
  TEN_ASSERT(engine_loop, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In the closing flow, the closing of the engine is always
  // after the closing of the extension thread, so its thread safe to access the
  // runloop of the engine here.
  TEN_ASSERT(ten_runloop_check_integrity(engine_loop, false),
             "Invalid use of engine's runloop %p.", engine_loop);

  ten_extension_thread_set_state(self, TEN_EXTENSION_THREAD_STATE_CLOSED);

  int rc = ten_runloop_post_task_tail(
      engine_loop, ten_engine_on_extension_thread_closed_task, engine, self);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

ten_runloop_t *ten_extension_thread_get_attached_runloop(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // threads other than the extension thread itself.
      ten_extension_thread_check_integrity(self, false), "Should not happen.");

  return self->runloop;
}

static void ten_extension_thread_inherit_thread_ownership(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The correct threading ownership will be setup
  // soon, so we can _not_ check thread safety here.
  TEN_ASSERT(ten_extension_thread_check_integrity(self, false),
             "Invalid use extension thread %p.", self);

  // Move the ownership of the extension thread relevant resources to the
  // belonging extension thread.
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);
  ten_sanitizer_thread_check_inherit_from(&self->extension_store->thread_check,
                                          &self->thread_check);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The correct threading ownership will be setup
  // soon, so we can _not_ check thread safety here.
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, false),
             "Invalid use extension group %p.", extension_group);

  ten_sanitizer_thread_check_inherit_from(&extension_group->thread_check,
                                          &self->thread_check);
  ten_sanitizer_thread_check_inherit_from(
      &extension_group->ten_env->thread_check, &self->thread_check);
}

static void *ten_extension_thread_main_actual(ten_extension_thread_t *self) {
  TEN_LOGD("Extension thread is started");

  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: The correct threading ownership will be setup
      // soon, so we can _not_ check thread safety here.
      ten_extension_thread_check_integrity(self, false), "Should not happen.");

  ten_extension_thread_inherit_thread_ownership(self);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_string_t extension_group_name;
  TEN_STRING_INIT(extension_group_name);

  ten_string_set_from_c_str(&extension_group_name, ten_extension_group_get_name(
                                                       extension_group, true));

  // The runloop should be created in its own thread.
  self->runloop = ten_runloop_create(NULL);
  TEN_ASSERT(self->runloop, "Should not happen.");

  int rc = ten_runloop_post_task_tail(
      self->runloop, ten_extension_thread_handle_start_msg_task, self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  // Before actually starting the extension thread's runloop, first notify the
  // engine (extension_context) that the extension thread's runloop is ready for
  // use.
  ten_event_set(self->runloop_is_ready_to_use);

  // Run the extension thread event loop.
  ten_runloop_run(self->runloop);

  TEN_LOGD("[%s] Notifying engine that we are closed.",
           ten_string_get_raw_str(&extension_group_name));

  ten_extension_thread_notify_engine_we_are_closed(self);

  TEN_LOGD("[%s] Extension thread is exited.",
           ten_string_get_raw_str(&extension_group_name));

  ten_string_deinit(&extension_group_name);

  return NULL;
}

// This is the extension thread.
static void *ten_extension_thread_main(void *self_) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  return ten_extension_thread_main_actual(self);
}

void ten_extension_thread_start(ten_extension_thread_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: because the extension thread has not started
                 // yet, we can _not_ check thread safety here.
                 ten_extension_thread_check_integrity(self, false),
             "Should not happen.");

  ten_thread_create(ten_string_get_raw_str(&self->extension_group->name),
                    ten_extension_thread_main, self);

  // The runloop of the extension_thread is created within the extension thread
  // itself, which introduces a time gap. If the engine (extension_context)
  // attempts to post a task to the runloop of extension_thread before the
  // runloop has been created, it would result in a segmentation fault since the
  // runloop would still be NULL. There are two approaches to handle this
  // situation:
  //
  // 1) Protect both the extension_thread and engine access to
  //    extension_thread::runloop with a mutex. But this is too heavy.
  // 2) The approach adopted here is to have the engine thread wait briefly
  //    until the runloop is successfully created by the extension_thread before
  //    proceeding. This eliminates the need to lock every time the runloop is
  //    accessed.
  ten_event_wait(self->runloop_is_ready_to_use, -1);
}

static void ten_extension_thread_on_triggering_close(void *self_,
                                                     TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  // The closing flow should be executed only once.
  if (self->is_close_triggered) {
    return;
  }

  self->is_close_triggered = true;

  switch (self->state) {
  case TEN_EXTENSION_THREAD_STATE_INIT:
    // Enter the deinit flow of the extension group directly.
    ten_extension_group_on_deinit(self->extension_group);
    break;

  case TEN_EXTENSION_THREAD_STATE_CREATING_EXTENSIONS:
    // We need to wait until `on_create_extensions_done()` is called, as that
    // is the point when all the created extensions can be retrieved to begin
    // the close process. Otherwise, memory leaks caused by those extensions
    // may occur.
    break;

  case TEN_EXTENSION_THREAD_STATE_NORMAL:
    ten_extension_thread_stop_life_cycle_of_all_extensions(self);
    break;

  case TEN_EXTENSION_THREAD_STATE_PREPARE_TO_CLOSE:
  case TEN_EXTENSION_THREAD_STATE_CLOSED:
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

void ten_extension_thread_close(ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is intended to be called in any threads.
  TEN_ASSERT(ten_extension_thread_check_integrity(self, false),
             "Should not happen.");

  TEN_LOGD("[%s] Try to close extension thread.",
           ten_extension_group_get_name(self->extension_group, false));

  // Notify extension thread that it is about to close.
  int rc = ten_runloop_post_task_tail(
      self->runloop, ten_extension_thread_on_triggering_close, self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

bool ten_extension_thread_call_by_me(ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: this function is intended to be called in any
      // threads.
      ten_extension_thread_check_integrity(self, false), "Should not happen.");

  return ten_thread_equal(NULL, ten_sanitizer_thread_check_get_belonging_thread(
                                    &self->thread_check));
}

bool ten_extension_thread_not_call_by_me(ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: this function is intended to be called in any
      // threads.
      ten_extension_thread_check_integrity(self, false), "Should not happen.");

  return !ten_extension_thread_call_by_me(self);
}

TEN_EXTENSION_THREAD_STATE
ten_extension_thread_get_state(ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  return self->state;
}

void ten_extension_thread_set_state(ten_extension_thread_t *self,
                                    TEN_EXTENSION_THREAD_STATE state) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  self->state = state;
}

static void ten_extension_thread_add_extension(ten_extension_thread_t *self,
                                               ten_extension_t *extension) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  extension->extension_thread = self;

  TEN_UNUSED bool rc =
      ten_extension_store_add_extension(self->extension_store, extension);
  TEN_ASSERT(rc, "Should not happen.");
}

void ten_extension_thread_stop_life_cycle_of_all_extensions_task(
    void *self, TEN_UNUSED void *arg) {
  ten_extension_thread_t *extension_thread = self;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid argument.");

  ten_extension_thread_stop_life_cycle_of_all_extensions(extension_thread);
}

/**
 * Begin processing all lifecycle stages of the extensions contained within the
 * extension thread. This means starting to invoke each extension's series of
 * lifecycle methods, beginning with `on_configure`.
 */
void ten_extension_thread_start_life_cycle_of_all_extensions_task(
    void *self_, TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid argument.");

  // The extension system is about to be shut down, so do not proceed with
  // initialization anymore.
  if (self->is_close_triggered) {
    return;
  }

  ten_extension_thread_set_state(self, TEN_EXTENSION_THREAD_STATE_NORMAL);

  // From here, it begins calling a series of lifecycle methods for the
  // extension, starting with `on_configure`.

  ten_list_foreach (&self->extensions, iter) {
    ten_extension_t *extension = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    ten_extension_load_metadata(extension);
  }
}

void ten_extension_thread_add_all_created_extensions(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: We are in the extension thread, and throughout the entire
      // lifecycle of the extension, the extension_context where the extension
      // resides remains unchanged. Even in the closing flow, the
      // extension_context is closed later than the extension itself. Therefore,
      // using a pointer to the extension_context within the extension thread is
      // thread-safe.
      ten_extension_context_check_integrity(extension_context, false),
      "Should not happen.");

  ten_list_foreach (&self->extensions, iter) {
    ten_extension_t *extension = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(extension, "Should not happen.");
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");

    // Correct the belonging_thread of the contained path_table.
    ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
        &extension->path_table->thread_check);

    ten_extension_thread_add_extension(self, extension);
  }

  // Notify the engine to handle those newly created extensions.

  ten_engine_t *engine = extension_context->engine;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The runloop of the engine will not be changed during the
  // whole lifetime of the extension thread, so it's thread safe to access it
  // here.
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  int rc = ten_runloop_post_task_tail(
      ten_engine_get_attached_runloop(engine),
      ten_engine_find_extension_info_for_all_extensions_of_extension_thread_task,
      engine, self);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}
