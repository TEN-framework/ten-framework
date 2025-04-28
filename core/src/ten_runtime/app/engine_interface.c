//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/engine_interface.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static void ten_app_check_termination_when_engine_closed_(void *app_,
                                                          void *engine_) {
  ten_app_t *app = (ten_app_t *)app_;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_engine_t *engine = (ten_engine_t *)engine_;

  ten_app_check_termination_when_engine_closed(app, engine);
}

static void ten_app_check_termination_when_engine_closed_async(
    ten_app_t *self, ten_engine_t *engine) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in
                 // different threads.
                 ten_app_check_integrity(self, false),
             "Should not happen.");

  int rc = ten_runloop_post_task_tail(
      ten_app_get_attached_runloop(self),
      ten_app_check_termination_when_engine_closed_, self, engine);
  TEN_ASSERT(!rc, "Should not happen.");
}

// This function is called in the engine thread.
static void ten_app_on_engine_closed(ten_engine_t *engine,
                                     TEN_UNUSED void *on_close_data) {
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_app_t *app = engine->app;
  TEN_ASSERT(app, "Invalid argument.");

  if (engine->has_own_loop) {
    // This is in the engine thread.
    TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

    // Because the app needs some information of an engine to do some
    // clean-up, so we can not destroy the engine now. We have to push the
    // engine to the specified list to wait for clean-up from the app.
    ten_app_check_termination_when_engine_closed_async(app, engine);
  } else {
    // This is in the app+thread thread.
    TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
    TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

    ten_app_check_termination_when_engine_closed(app, engine);
  }
}

static void ten_app_add_engine(ten_app_t *self, ten_engine_t *engine) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(engine, "Should not happen.");

  ten_list_push_ptr_back(&self->engines, engine, NULL);
  ten_engine_set_on_closed(engine, ten_app_on_engine_closed, NULL);
}

/**
 * @brief Creates a new engine instance and adds it to the app's engine list.
 *
 * This function creates a new engine instance based on the provided command
 * (typically a start_graph command) and registers it with the app. The engine
 * will be associated with the app and will have its lifecycle managed by the
 * app.
 *
 * The engine may either share the app's runloop or create its own thread and
 * runloop, depending on the configuration. When the engine is closed, it will
 * trigger the app's engine closure handling via the registered callback.
 *
 * @param self The app instance that will own the engine.
 * @param cmd The command (typically a start_graph command) that contains
 *            configuration information for the new engine, including graph ID
 *            and long-running mode settings.
 *
 * @return A pointer to the newly created engine instance.
 *
 * @note This function must be called from the app's main thread to ensure
 *       thread safety when adding the engine to the app's engine list.
 */
ten_engine_t *ten_app_create_engine(ten_app_t *self, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");

  TEN_LOGD("[%s] App creates an engine.", ten_app_get_uri(self));

  ten_engine_t *engine = ten_engine_create(self, cmd);
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_app_add_engine(self, engine);

  return engine;
}

/**
 * @brief Removes an engine from the app's engine list.
 *
 * This function removes the specified engine from the app's list of engines.
 * It does not destroy the engine object itself - that responsibility is handled
 * elsewhere, typically by decrementing the engine's reference count after
 * ensuring it's safe to do so.
 *
 * @param self The app instance that owns the engine.
 * @param engine The engine to be removed from the app.
 *
 * @note This function must only be called from the app's main thread.
 *       It is not thread-safe and assumes the caller has ensured proper
 *       synchronization if the engine had its own thread.
 */
void ten_app_del_engine(ten_app_t *self, ten_engine_t *engine) {
  TEN_ASSERT(self && ten_app_check_integrity(self, true) && engine,
             "Should not happen.");

  TEN_LOGD("[%s:%s] Remove engine from app.", ten_app_get_uri(self),
           ten_engine_get_id(engine, false));

  // This operation must be performed in the app's main thread.
  // No locks are needed because we enforce thread safety through
  // proper thread management rather than mutex protection.
  ten_list_remove_ptr(&self->engines, engine);
}

static ten_engine_t *ten_app_get_engine_by_graph_id_internal(
    ten_app_t *self, const char *graph_id) {
  TEN_ASSERT(self && ten_app_check_integrity(self, true) && graph_id,
             "Should not happen.");

  ten_list_foreach (&self->engines, iter) {
    ten_engine_t *engine = ten_ptr_listnode_get(iter.node);

    if (ten_c_string_is_equal(ten_string_get_raw_str(&engine->graph_id),
                              graph_id)) {
      return engine;
    }
  }

  return NULL;
}

/**
 * @brief Retrieves information about a singleton predefined graph by its name.
 *
 * This function searches for a singleton predefined graph with the specified
 * name in the app's list of predefined graphs. A singleton graph is one that
 * can have only one instance running at a time within the application.
 *
 * @param self The app instance containing the predefined graph information.
 * @param graph_name The name of the singleton predefined graph to find.
 *
 * @return A pointer to the predefined graph information if found, NULL
 * otherwise.
 */
ten_predefined_graph_info_t *
ten_app_get_singleton_predefined_graph_info_by_name(ten_app_t *self,
                                                    const char *graph_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(graph_name, "Invalid graph_name pointer.");

  if (strlen(graph_name) == 0) {
    // Empty graph name, cannot identify which predefined graph to retrieve.
    return NULL;
  }

  // Call the actual implementation from predefined_graph.c
  return ten_predefined_graph_infos_get_singleton_by_name(
      &self->predefined_graph_infos, graph_name);
}

ten_engine_t *ten_app_get_engine_by_graph_id(ten_app_t *self,
                                             const char *graph_id) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(graph_id, "Invalid graph_id pointer.");

  if (strlen(graph_id) == 0) {
    // There are no destination information in the message, so we don't know
    // which engine this message should go.
    return NULL;
  }

  if (ten_raw_string_is_uuid4(graph_id)) {
    return ten_app_get_engine_by_graph_id_internal(self, graph_id);
  }

  // As a last resort, since there can only be one instance of a singleton graph
  // within a process, the engine instance of the singleton graph can be found
  // through the graph_name.
  return ten_app_get_singleton_predefined_graph_engine_by_name(self, graph_id);
}
