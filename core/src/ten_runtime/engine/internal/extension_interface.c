//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/internal/extension_interface.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/remote_interface.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/engine/msg_interface/start_graph.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

// The 'cmd' parameter is the command triggers the enabling of extension system.
// If there is no command to trigger the enabling, this parameter would be NULL.
bool ten_engine_enable_extension_system(ten_engine_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_shared_ptr_t *original_start_graph_cmd =
      self->original_start_graph_cmd_of_enabling_engine;
  TEN_ASSERT(original_start_graph_cmd &&
                 ten_msg_check_integrity(original_start_graph_cmd),
             "Should not happen.");

  if (self->is_closing) {
    TEN_LOGE("Engine is closing, do not enable extension system.");

    ten_engine_return_error_for_cmd_start_graph(
        self, original_start_graph_cmd,
        "Engine is closing, do not enable extension system.");

    ten_shared_ptr_destroy(original_start_graph_cmd);
    self->original_start_graph_cmd_of_enabling_engine = NULL;

    return false;
  }

  if (self->extension_context) {
    // The engine has already started a extension execution context, so
    // returning OK directly.
    ten_engine_return_ok_for_cmd_start_graph(self, original_start_graph_cmd);

    ten_shared_ptr_destroy(original_start_graph_cmd);
    self->original_start_graph_cmd_of_enabling_engine = NULL;
  } else {
    self->extension_context = ten_extension_context_create(self);
    ten_extension_context_set_on_closed(
        self->extension_context, ten_engine_on_extension_context_closed, self);

    if (!ten_extension_context_start_extension_group(self->extension_context,
                                                     err)) {
      TEN_LOGE("[%s] Failed to start the extension system.",
               ten_app_get_uri(self->app));

      ten_engine_return_error_for_cmd_start_graph(
          self, original_start_graph_cmd,
          "[%s] Failed to start the extension system.",
          ten_app_get_uri(self->app));

      ten_shared_ptr_destroy(original_start_graph_cmd);
      self->original_start_graph_cmd_of_enabling_engine = NULL;

      return false;
    }
  }

  return true;
}

/**
 * After the initialization of all extension threads in the engine (representing
 * a graph) is completed (regardless of whether the result is success or
 * failure), the engine needs to respond to the original requester of the graph
 * creation (i.e., a `start_graph` command) with a result.
 */
static void ten_engine_on_all_extension_threads_are_ready(
    ten_engine_t *self, ten_extension_thread_t *extension_thread) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(
      extension_thread &&
          // TEN_NOLINTNEXTLINE(thread-check)
          // thread-check: this function does not access this extension_thread,
          // we just check if the arg is an ten_extension_thread_t.
          ten_extension_thread_check_integrity(extension_thread, false),
      "Should not happen.");

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context &&
                 ten_extension_context_check_integrity(extension_context, true),
             "Should not happen.");

  extension_context->extension_threads_cnt_of_ready++;
  if (extension_context->extension_threads_cnt_of_ready ==
      ten_list_size(&extension_context->extension_threads)) {
    bool error_occurred = false;

    // Check if there were any errors during the creation and/or initialization
    // of any extension thread/group. If errors are found, shut down the
    // engine/graph and return the corresponding result to the original
    // requester.

    ten_list_foreach (&extension_context->extension_threads, iter) {
      ten_extension_thread_t *extension_thread =
          ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(extension_thread, "Should not happen.");
      TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, false),
                 "Should not happen.");

      ten_extension_group_t *extension_group =
          extension_thread->extension_group;
      TEN_ASSERT(extension_group, "Should not happen.");
      TEN_ASSERT(ten_extension_group_check_integrity(extension_group, false),
                 "Should not happen.");

      if (!ten_error_is_success(&extension_group->err_before_ready)) {
        error_occurred = true;
        break;
      }
    }

    ten_shared_ptr_t *original_start_graph_cmd =
        self->original_start_graph_cmd_of_enabling_engine;
    TEN_ASSERT(original_start_graph_cmd &&
                   ten_msg_check_integrity(original_start_graph_cmd),
               "Should not happen.");

    ten_shared_ptr_t *cmd_result = NULL;
    if (error_occurred) {
      TEN_LOGE("[%s] Failed to start the graph successfully, shutting it down.",
               ten_engine_get_id(self, true));

      cmd_result = ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR,
                                                  original_start_graph_cmd);
    } else {
      TEN_LOGV("[%s] All extension threads are initted.",
               ten_app_get_uri(self->app));

      ten_string_t *graph_id = &self->graph_id;

      const char *body_str =
          ten_string_is_empty(graph_id) ? "" : ten_string_get_raw_str(graph_id);

      cmd_result = ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_OK,
                                                  original_start_graph_cmd);
      ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                           ten_value_create_string(body_str), NULL);

      // Mark the engine that it could start to handle messages.
      self->is_ready_to_handle_msg = true;

      TEN_LOGD("[%s] Engine is ready to handle messages.",
               ten_app_get_uri(self->app));
    }

    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);

    ten_shared_ptr_destroy(original_start_graph_cmd);
    self->original_start_graph_cmd_of_enabling_engine = NULL;

    if (error_occurred) {
      ten_app_t *app = self->app;
      TEN_ASSERT(app && ten_app_check_integrity(app, false),
                 "Invalid argument.");

      // This graph/engine will not be functioning properly, so it will be shut
      // down directly.
      ten_shared_ptr_t *stop_graph_cmd = ten_cmd_stop_graph_create();
      ten_msg_clear_and_set_dest(stop_graph_cmd, ten_app_get_uri(app),
                                 ten_engine_get_id(self, false), NULL, NULL,
                                 NULL);

      ten_env_send_cmd(self->ten_env, stop_graph_cmd, NULL, NULL, NULL, NULL);

      ten_shared_ptr_destroy(stop_graph_cmd);
    } else {
      // Because the engine is just ready to handle messages, hence, we trigger
      // the engine to handle any _pending_/_cached_ external messages if any.
      ten_engine_handle_in_msgs_async(self);
    }
  }
}

void ten_engine_find_extension_info_for_all_extensions_of_extension_thread_task(
    void *self_, void *arg) {
  ten_engine_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context &&
                 ten_extension_context_check_integrity(extension_context, true),
             "Should not happen.");

  TEN_UNUSED ten_extension_thread_t *extension_thread = arg;
  TEN_ASSERT(extension_thread &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function does not access this
                 // extension_thread, we just check if the arg is an
                 // ten_extension_thread_t.
                 ten_extension_thread_check_integrity(extension_thread, false),
             "Should not happen.");

  ten_list_foreach (&extension_thread->extensions, iter) {
    ten_extension_t *extension = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(ten_extension_check_integrity(extension, false),
               "Should not happen.");

    // Setup 'extension_context' field, this is the most important field when
    // extension is initiating.
    extension->extension_context = extension_context;

    // Find the extension_info of the specified 'extension'.
    extension->extension_info =
        ten_extension_context_get_extension_info_by_name(
            extension_context, ten_app_get_uri(extension_context->engine->app),
            ten_engine_get_id(extension_context->engine, true),
            ten_extension_group_get_name(extension_thread->extension_group,
                                         false),
            ten_extension_get_name(extension, false));
  }

  if (extension_thread->is_close_triggered) {
    int rc = ten_runloop_post_task_tail(
        ten_extension_thread_get_attached_runloop(extension_thread),
        ten_extension_thread_stop_life_cycle_of_all_extensions_task,
        extension_thread, NULL);

    if (rc) {
      TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
  } else {
    ten_engine_on_all_extension_threads_are_ready(self, extension_thread);

    int rc = ten_runloop_post_task_tail(
        ten_extension_thread_get_attached_runloop(extension_thread),
        ten_extension_thread_start_life_cycle_of_all_extensions_task,
        extension_thread, NULL);

    if (rc) {
      TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
  }
}
