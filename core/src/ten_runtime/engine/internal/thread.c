//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/internal/thread.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TIMEOUT_FOR_ENGINE_THREAD_STARTED 5000  // ms

/**
 * @brief Main function for the engine thread.
 *
 * This function is the entry point for a dedicated engine thread.
 *
 * @param self_ Pointer to the engine instance (cast to void*).
 * @return NULL when the thread completes.
 */
static void *ten_engine_thread_main(void *self_) {
  ten_engine_t *self = (ten_engine_t *)self_;
  TEN_ASSERT(self, "Should not happen.");

  // Initialize thread checking for this engine thread.
  ten_sanitizer_thread_check_set_belonging_thread(&self->thread_check, NULL);
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  // Update thread ownership for the path table.
  // The path table was created in the original thread (e.g., the app thread),
  // so we need to transfer ownership to the current engine thread.
  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &self->path_table->thread_check);

  TEN_LOGD(
      "[%s] Engine thread %p is started.", ten_engine_get_id(self, true),
      ten_sanitizer_thread_check_get_belonging_thread(&self->thread_check));

  // Create the ten_env environment for the engine on the engine thread.
  TEN_ASSERT(!self->ten_env, "ten_env should be NULL at this point");
  self->ten_env = ten_env_create_for_engine(self);

  // Create a dedicated event loop for this engine.
  self->loop = ten_runloop_create(NULL);
  ten_event_set(self->runloop_is_created);

  // Run the event loop - this call blocks until the engine is about to close.
  ten_runloop_run(self->loop);

  // Execute the on_closed callback if registered.
  if (self->on_closed) {
    self->on_closed(self, self->on_closed_data);
  }

  TEN_LOGD("[%s] Engine thread is stopped.", ten_engine_get_id(self, true));

  return NULL;
}

/**
 * Creates a dedicated thread for the engine to run its own event loop.
 *
 * The engine thread needs to be fully initialized before we can transfer
 * file descriptors or other resources to it.
 *
 * @param self Pointer to the engine instance.
 */
void ten_engine_create_its_own_thread(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Should not happen.");

  // Create synchronization events for coordinating with the new thread.
  self->runloop_is_created = ten_event_create(0, 0);

  // Create a new thread using the graph_id as the thread name.
  ten_thread_create(ten_string_get_raw_str(&self->graph_id),
                    ten_engine_thread_main, self);

  // Wait indefinitely for the engine's event loop to be created.
  TEN_UNUSED int rc = ten_event_wait(self->runloop_is_created,
                                     TIMEOUT_FOR_ENGINE_THREAD_STARTED);
  TEN_ASSERT(!rc, "Should not happen.");
}

void ten_engine_init_individual_eventloop_relevant_vars(ten_engine_t *self,
                                                        ten_app_t *app) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  // Each engine can have its own decision of having its own eventloop. We
  // currently use a simplified strategy to determine this decision of every
  // engines. If we have a more complex policy decision in the future, just
  // modify the following line is enough.
  if (app->one_event_loop_per_engine) {
    self->has_own_loop = true;
  } else {
    self->has_own_loop = false;
  }
}
