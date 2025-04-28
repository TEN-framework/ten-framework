//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/engine/internal/close.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"

#define TEN_ENGINE_SIGNATURE 0x68E43695C0DB905AU

#define CMD_ID_COUNTER_MIN_VALUE 0
#define CMD_ID_COUNTER_MAX_VALUE 4095

typedef struct ten_extension_context_t ten_extension_context_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_env_t ten_env_t;
typedef struct ten_connection_t ten_connection_t;

struct ten_engine_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_ref_t ref;
  bool is_closing;

  // Whether the engine has an uncompleted async task.
  bool has_uncompleted_async_task;

  ten_engine_on_closed_func_t on_closed;
  void *on_closed_data;

  ten_app_t *app;
  ten_extension_context_t *extension_context;

  ten_env_t *ten_env;

  // This means that the engine can start to handle messages, i.e. all the
  // extension threads are started successfully.
  //
  // The reason this is needed is that if messages are imported into the engine
  // too early, before the extension group and extensions have even been
  // created, there is a possibility that those extension groups and extensions,
  // which will be created later, cannot be found. This could lead to message
  // dispatch failures. Therefore, this is needed to hold off importing messages
  // into the engine until it is fully prepared.
  bool is_ready_to_handle_msg;

  // When app creates an engine, it will create a randomized graph ID for the
  // engine. It _must_ be a UUID4 string.
  ten_string_t graph_id;

  ten_path_table_t *path_table;

  // Save the original received 'start_graph' command so that after we
  // successfully started the engine, we can return a correct cmd result back
  // according to this saved 'start_graph' command.
  ten_shared_ptr_t *original_start_graph_cmd_of_enabling_engine;

  ten_list_t timers;

  // Connections that are not connected to the remote.
  ten_list_t orphan_connections;

  // @{
  ten_hashtable_t remotes;  // ten_remote_t
  ten_list_t weak_remotes;
  // @}

  // @{
  // Used to send messages to the engine.
  ten_mutex_t *in_msgs_lock;
  ten_list_t in_msgs;
  // @}

  // @{
  // The following members are used for engines which have its own event loop.
  bool has_own_loop;
  ten_runloop_t *loop;
  ten_event_t *runloop_is_created;
  // @}

  bool long_running_mode;

  // Store the stop_graph command that will shut down this engine temporarily,
  // so that after the engine has completely closed, the cmd_result can be
  // returned based on this.
  ten_shared_ptr_t *cmd_stop_graph;
};

TEN_RUNTIME_PRIVATE_API bool ten_engine_check_integrity(ten_engine_t *self,
                                                        bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_engine_t *ten_engine_create(ten_app_t *app,
                                                        ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_engine_get_attached_runloop(
    ten_engine_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_engine_is_ready_to_handle_msg(
    ten_engine_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_engine_get_id(ten_engine_t *self,
                                                      bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_engine_add_orphan_connection(
    ten_engine_t *self, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API void ten_engine_del_orphan_connection(
    ten_engine_t *self, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API ten_connection_t *ten_engine_find_orphan_connection(
    ten_engine_t *self, const char *uri);
