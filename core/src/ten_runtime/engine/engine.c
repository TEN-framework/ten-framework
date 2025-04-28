//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/engine.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/internal/extension_interface.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/start_graph/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uuid.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

bool ten_engine_check_integrity(ten_engine_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ENGINE_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

static void ten_engine_destroy(ten_engine_t *self) {
  TEN_ASSERT(
      self &&
          // TEN_NOLINTNEXTLINE(thread-check)
          // thread-check: The belonging thread of the 'engine' is ended when
          // this function is called, so we can not check thread integrity here.
          ten_engine_check_integrity(self, false),
      "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Should not happen.");

  // The engine could only be destroyed when there is no extension threads,
  // no prev/next remote apps (connections), no timers associated with it.
  TEN_ASSERT(
      (self->extension_context == NULL) && ten_list_is_empty(&self->timers),
      "Should not happen.");

  TEN_LOGD("[%s] Destroy engine.", ten_engine_get_id(self, false));

  ten_env_destroy(self->ten_env);

  ten_signature_set(&self->signature, 0);

  TEN_ASSERT(ten_list_is_empty(&self->orphan_connections),
             "Should not happen.");

  ten_hashtable_deinit(&self->remotes);
  ten_list_clear(&self->weak_remotes);

  ten_mutex_destroy(self->in_msgs_lock);
  ten_list_clear(&self->in_msgs);

  if (self->has_own_loop) {
    ten_event_destroy(self->runloop_is_created);

    ten_runloop_destroy(self->loop);
    self->loop = NULL;
  }

  if (self->original_start_graph_cmd_of_enabling_engine) {
    ten_shared_ptr_destroy(self->original_start_graph_cmd_of_enabling_engine);
    self->original_start_graph_cmd_of_enabling_engine = NULL;
  }

  if (self->cmd_stop_graph) {
    ten_shared_ptr_destroy(self->cmd_stop_graph);
    self->cmd_stop_graph = NULL;
  }

  ten_string_deinit(&self->graph_id);

  ten_path_table_destroy(self->path_table);

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  ten_ref_deinit(&self->ref);

  TEN_FREE(self);
}

// graph_id is the identify of one graph, so the graph_id in all related
// engines MUST be the same. graph_id will be generated in the first app, and
// will transfer with the message to the next app.
static void ten_engine_set_graph_id(ten_engine_t *self, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd && ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_START_GRAPH,
             "Should not happen.");

  const ten_string_t *src_uri = &ten_msg_get_src_loc(cmd)->app_uri;
  const ten_string_t *src_graph_id = &ten_msg_get_src_loc(cmd)->graph_id;

  // One app could not have two engines with the same graph_id, so only when
  // the command is from another app, we can use the graph_id attached in that
  // command to be the graph_id of the newly created engine.
  if (!ten_string_is_equal(src_uri, &self->app->uri) &&
      !ten_string_is_empty(src_graph_id)) {
    TEN_LOGD("[%s] Inherit engine's name from previous node.",
             ten_string_get_raw_str(src_graph_id));
    ten_string_init_formatted(&self->graph_id, "%s",
                              ten_string_get_raw_str(src_graph_id));
  } else {
    ten_string_t graph_id_str;
    TEN_STRING_INIT(graph_id_str);
    ten_uuid4_gen_string(&graph_id_str);

    // Set the newly created graph_id to the engine.
    ten_string_init_formatted(&self->graph_id, "%s",
                              ten_string_get_raw_str(&graph_id_str));

    // Set the newly created graph_id to the 'start_graph' command.
    ten_list_foreach (ten_msg_get_dest(cmd), iter) {
      ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(dest_loc && ten_loc_check_integrity(dest_loc),
                 "Should not happen.");

      ten_string_set_formatted(&dest_loc->graph_id, "%s",
                               ten_string_get_raw_str(&graph_id_str));
    }

    ten_string_deinit(&graph_id_str);
  }

  // Got graph_id, update the graph_id field of all the extensions_info that
  // this start_graph command has.
  ten_cmd_start_graph_fill_loc_info(cmd, ten_app_get_uri(self->app),
                                    ten_engine_get_id(self, true));
}

bool ten_engine_is_ready_to_handle_msg(ten_engine_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  return self->is_ready_to_handle_msg;
}

static void ten_engine_on_end_of_life(ten_ref_t *ref, void *supervisee) {
  ten_engine_t *self = (ten_engine_t *)supervisee;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, false), "Should not happen.");

  ten_engine_destroy(self);
}

ten_engine_t *ten_engine_create(ten_app_t *app, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(app && ten_app_check_integrity(app, true) && cmd &&
                 ten_cmd_base_check_integrity(cmd),
             "Should not happen.");

  TEN_LOGD("Create engine.");

  ten_engine_t *self = (ten_engine_t *)TEN_MALLOC(sizeof(ten_engine_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_ENGINE_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  ten_ref_init(&self->ref, self, ten_engine_on_end_of_life);
  self->is_closing = false;
  self->has_uncompleted_async_task = false;
  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->app = app;
  self->extension_context = NULL;

  self->loop = NULL;
  self->runloop_is_created = NULL;
  self->is_ready_to_handle_msg = false;

  ten_list_init(&self->orphan_connections);

  ten_hashtable_init(&self->remotes,
                     offsetof(ten_remote_t, hh_in_remote_table));
  ten_list_init(&self->weak_remotes);

  ten_list_init(&self->timers);
  self->path_table =
      ten_path_table_create(TEN_PATH_TABLE_ATTACH_TO_ENGINE, self);

  self->in_msgs_lock = ten_mutex_create();
  ten_list_init(&self->in_msgs);

  self->original_start_graph_cmd_of_enabling_engine = NULL;
  self->cmd_stop_graph = NULL;

  self->ten_env = NULL;

  self->long_running_mode = ten_cmd_start_graph_get_long_running_mode(cmd);

  ten_engine_set_graph_id(self, cmd);

  ten_engine_init_individual_eventloop_relevant_vars(self, app);
  if (self->has_own_loop) {
    ten_engine_create_its_own_thread(self);
  } else {
    // Since the engine does not have its own run loop, it means that it will
    // reuse the app's run loop. Therefore, the current app thread is also the
    // engine thread, allowing us to create the ten_env object here.
    self->ten_env = ten_env_create_for_engine(self);
  }

  return self;
}

ten_runloop_t *ten_engine_get_attached_runloop(ten_engine_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in
                 // different threads.
                 ten_engine_check_integrity(self, false),
             "Should not happen.");

  if (self->has_own_loop) {
    return self->loop;
  } else {
    return ten_app_get_attached_runloop(self->app);
  }
}

const char *ten_engine_get_id(ten_engine_t *self, bool check_thread) {
  TEN_ASSERT(self && ten_engine_check_integrity(self, check_thread),
             "Should not happen.");

  return ten_string_get_raw_str(&self->graph_id);
}

void ten_engine_del_orphan_connection(ten_engine_t *self,
                                      ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  TEN_LOGD("[%s] Remove a orphan connection %p", ten_engine_get_id(self, true),
           connection);

  TEN_UNUSED bool rc =
      ten_list_remove_ptr(&self->orphan_connections, connection);
  TEN_ASSERT(rc, "Should not happen.");

  connection->on_closed = NULL;
  connection->on_closed_data = NULL;
}

static void ten_engine_on_orphan_connection_closed(
    ten_connection_t *connection, TEN_UNUSED void *on_closed_data) {
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  ten_engine_t *self = connection->attached_target.engine;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  TEN_LOGD("[%s] Orphan connection %p closed", ten_engine_get_id(self, true),
           connection);

  ten_engine_del_orphan_connection(self, connection);
  ten_connection_destroy(connection);

  // Check if the app is in the closing phase.
  if (self->is_closing) {
    TEN_LOGD("[%s] Engine is closing, check to see if it could proceed.",
             ten_engine_get_id(self, true));
    ten_engine_on_close(self);
  } else {
    // If 'connection' is an orphan connection, it means the connection is not
    // attached to an engine, and the TEN app should _not_ be closed due to an
    // strange connection like this, otherwise, the TEN app will be very
    // fragile, anyone could simply connect to the TEN app, and close the app
    // through disconnection.
  }
}

void ten_engine_add_orphan_connection(ten_engine_t *self,
                                      ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  TEN_LOGD("[%s] Add a orphan connection %p[%s] (total cnt %zu)",
           ten_engine_get_id(self, true), connection,
           ten_string_get_raw_str(&connection->uri),
           ten_list_size(&self->orphan_connections));

  ten_connection_set_on_closed(connection,
                               ten_engine_on_orphan_connection_closed, NULL);

  // Do not set 'ten_connection_destroy' as the destroy function, because we
  // might _move_ a connection out of 'orphan_connections' list when it is
  // associated with an engine.
  ten_list_push_ptr_back(&self->orphan_connections, connection, NULL);
}

ten_connection_t *ten_engine_find_orphan_connection(ten_engine_t *self,
                                                    const char *uri) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  if (strlen(uri)) {
    ten_list_foreach (&self->orphan_connections, iter) {
      ten_connection_t *connection = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
                 "Should not happen.");

      if (ten_string_is_equal_c_str(&connection->uri, uri)) {
        return connection;
      }
    }
  }

  return NULL;
}
