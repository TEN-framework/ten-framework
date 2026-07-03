//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stdbool.h>
#include <stddef.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/base_dir.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/app/metadata.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/app/service_hub/service_hub.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

bool ten_app_check_integrity(ten_app_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_APP_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_app_handle_metadata_task(void *self_, void *arg) {
  ten_app_t *self = (ten_app_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid use of app %p.",
             self);

  ten_app_handle_metadata(self);
}

/**
 * @brief Starts the TEN app and runs its event loop.
 *
 * The app will continue running until ten_runloop_stop() is called,
 * typically during the app shutdown process.
 *
 * @param self Pointer to the app instance
 *
 * @note Thread-safety: This function must be called from the thread that owns
 * the app instance.
 */
void ten_app_start(ten_app_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid use of app %p.",
             self);

  ten_app_find_and_set_base_dir(self);

  // Post the initial metadata handling task to start the application lifecycle.
  int rc = ten_runloop_post_task_tail(self->loop, ten_app_handle_metadata_task,
                                      self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post initial metadata task: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
    return;
  }

  // Start the app's event loop - this call blocks until the loop is stopped.
  ten_runloop_run(self->loop);

  TEN_LOGD("TEN app runloop ends");

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  // Clean up telemetry system after the runloop exits.
  ten_app_deinit_service_hub(self);
#endif
}

void ten_app_add_orphan_connection(ten_app_t *self,
                                   ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(connection, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(connection, true),
             "Should not happen.");

  TEN_LOGD("[%s] Add a orphan connection %p (total cnt %zu)",
           ten_app_get_uri(self), connection,
           ten_list_size(&self->orphan_connections));

  ten_connection_set_on_closed(connection, ten_app_on_orphan_connection_closed,
                               NULL);

  // Do not set 'ten_connection_destroy' as the destroy function, because we
  // might _move_ a connection out of 'orphan_connections' list when it is
  // associated with an engine.
  ten_list_push_ptr_back(&self->orphan_connections, connection, NULL);
}

void ten_app_del_orphan_connection(ten_app_t *self,
                                   ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is always called in the app thread, however,
  // this function maybe called _after_ the connection has migrated to the
  // engine thread, so the connection belongs to the engine thread in that case.
  // And what we do here is just to check if the pointer points to a valid
  // connection instance, and we don't access the internal of the connection
  // instance here, so it is thread safe.
  TEN_ASSERT(connection, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(connection, false),
             "Should not happen.");

  TEN_LOGD("[%s] Remove a orphan connection %p", ten_app_get_uri(self),
           connection);

  TEN_UNUSED bool rc =
      ten_list_remove_ptr(&self->orphan_connections, connection);
  TEN_ASSERT(rc, "Should not happen.");

  connection->on_closed = NULL;
  connection->on_closed_data = NULL;
}

bool ten_app_has_orphan_connection(ten_app_t *self,
                                   ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(connection, "Should not happen.");
  TEN_ASSERT(ten_connection_check_integrity(connection, true),
             "Should not happen.");

  ten_listnode_t *found =
      ten_list_find_ptr(&self->orphan_connections, connection);

  return found != NULL;
}

ten_runloop_t *ten_app_get_attached_runloop(ten_app_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in different threads.
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");
  TEN_ASSERT(self->loop, "Should not happen.");

  return self->loop;
}

const char *ten_app_get_uri(ten_app_t *self) {
  // The app uri should be read-only after it has been set initially, so it's
  // safe to read it from any other threads.
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  return ten_string_get_raw_str(&self->uri);
}

ten_env_t *ten_app_get_ten_env(ten_app_t *self) {
  // The app ten_env should be read-only after it has been set initially, so
  // it's safe to read it from any other threads.
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  return self->ten_env;
}

ten_sanitizer_thread_check_t *ten_app_get_thread_check(ten_app_t *self) {
  // The app thread_check should be read-only after it has been set initially,
  // so it's safe to read it from any other threads.
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  return &self->thread_check;
}
