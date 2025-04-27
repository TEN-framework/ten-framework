//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_context/extension_context.h"

#include <stddef.h>
#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/extension_group_info/extension_group_info.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/start_graph/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

bool ten_extension_context_check_integrity(ten_extension_context_t *self,
                                           bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_CONTEXT_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

ten_extension_context_t *ten_extension_context_create(ten_engine_t *engine) {
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  TEN_LOGD("[%s] Create Extension context.", ten_engine_get_id(engine, true));

  ten_extension_context_t *self =
      (ten_extension_context_t *)TEN_MALLOC(sizeof(ten_extension_context_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_EXTENSION_CONTEXT_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->engine = engine;

  ten_list_init(&self->extension_groups_info_from_graph);
  ten_list_init(&self->extensions_info_from_graph);

  ten_list_init(&self->extension_threads);

  self->extension_threads_cnt_of_ready = 0;
  self->extension_threads_cnt_of_closed = 0;

  return self;
}

static void ten_extension_context_destroy(ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT((ten_list_size(&self->extension_threads) == 0),
             "Should not happen.");

  ten_list_clear(&self->extension_groups_info_from_graph);
  ten_list_clear(&self->extensions_info_from_graph);

  ten_signature_set(&self->signature, 0);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

static void ten_extension_context_start(ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  ten_list_foreach (&self->extension_threads, iter) {
    ten_extension_thread_start(ten_ptr_listnode_get(iter.node));
  }
}

static void
ten_extension_context_do_close_after_all_extension_threads_are_closed(
    ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  ten_engine_t *engine = self->engine;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_env_close(engine->ten_env);

  if (self->on_closed) {
    self->on_closed(self, self->on_closed_data);
  }

  ten_extension_context_destroy(self);
}

void ten_extension_context_close(ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);
  TEN_ASSERT(self->engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self->engine, true),
             "Should not happen.");

  TEN_LOGD("[%s] Try to close extension context.",
           ten_engine_get_id(self->engine, true));

  if (ten_list_is_empty(&self->extension_threads)) {
    // No extension threads need to be closed, so we can proceed directly to the
    // closing process of the extension context itself.
    ten_extension_context_do_close_after_all_extension_threads_are_closed(self);
    return;
  }

  self->extension_threads_cnt_of_closed = 0;
  self->extension_threads_total_cnt = ten_list_size(&self->extension_threads);

  ten_list_foreach (&self->extension_threads, iter) {
    ten_extension_thread_t *extension_thread = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(extension_thread && ten_extension_thread_check_integrity(
                                       extension_thread, false),
               "Should not happen.");

    ten_extension_thread_close(extension_thread);
  }
}

static bool ten_extension_context_could_be_close(
    ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  // Extension context could _only_ be closed when all extension threads have
  // been removed from the engine (extension context).

  return self->extension_threads_cnt_of_closed ==
         self->extension_threads_total_cnt;
}

static void ten_extension_context_do_close(ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT(ten_list_is_empty(&self->extension_threads), "Should not happen.");

  ten_extension_context_do_close_after_all_extension_threads_are_closed(self);
}

void ten_extension_context_on_close(ten_extension_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  if (!ten_extension_context_could_be_close(self)) {
    TEN_LOGD("[%s] Could not close alive extension context.",
             ten_engine_get_id(self->engine, true));
    return;
  }

  TEN_LOGD("[%s] Extension context can be closed now.",
           ten_engine_get_id(self->engine, true));

  ten_extension_context_do_close(self);
}

void ten_extension_context_set_on_closed(
    ten_extension_context_t *self,
    ten_extension_context_on_closed_func_t on_closed, void *on_closed_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}

ten_extension_info_t *ten_extension_context_get_extension_info_by_name(
    ten_extension_context_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_name) {
  TEN_ASSERT(self, "Invalid argument.");

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function will be called in the extension thread,
  // however, the graph would not be changed after the extension system is
  // starting, so it's safe to access the graph information in the extension
  // thead.
  //
  // However, for the strict thread safety, it's possible to modify the logic
  // here to use asynchronous operations (i.e., add a task to the
  // extension_context, and add a task to the extension_thread when the result
  // is found) here.
  TEN_ASSERT(ten_extension_context_check_integrity(self, false),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT(app_uri && extension_group_name && extension_name,
             "Should not happen.");

  ten_extension_info_t *result = NULL;

  ten_list_foreach (&self->extensions_info_from_graph, iter) {
    ten_extension_info_t *extension_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));

    if (app_uri &&
        !ten_string_is_equal_c_str(&extension_info->loc.app_uri, app_uri)) {
      continue;
    }

    if (graph_id &&
        !ten_string_is_equal_c_str(&extension_info->loc.graph_id, graph_id)) {
      continue;
    }

    if (extension_group_name &&
        !ten_string_is_equal_c_str(&extension_info->loc.extension_group_name,
                                   extension_group_name)) {
      continue;
    }

    if (extension_name &&
        !ten_string_is_equal_c_str(&extension_info->loc.extension_name,
                                   extension_name)) {
      continue;
    }

    result = extension_info;
    break;
  }

  return result;
}

static ten_extension_group_info_t *
ten_extension_context_get_extension_group_info_by_name(
    ten_extension_context_t *self, const char *app_uri,
    const char *extension_group_name) {
  TEN_ASSERT(self, "Invalid argument.");

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function will be called in the extension thread,
  // however, the graph would not be changed after the extension system is
  // starting, so it's safe to access the graph information in the extension
  // thead.
  //
  // However, for the strict thread safety, it's possible to modify the logic
  // here to use asynchronous operations (i.e., add a task to the
  // extension_context, and add a task to the extension_thread when the result
  // is found) here.
  TEN_ASSERT(ten_extension_context_check_integrity(self, false),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT(app_uri && extension_group_name, "Should not happen.");

  ten_extension_group_info_t *result = NULL;

  ten_list_foreach (&self->extension_groups_info_from_graph, iter) {
    ten_extension_group_info_t *extension_group_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));

    if (ten_string_is_equal_c_str(&extension_group_info->loc.app_uri,
                                  app_uri) &&
        ten_string_is_equal_c_str(
            &extension_group_info->loc.extension_group_name,
            extension_group_name)) {
      result = extension_group_info;
      break;
    }
  }

  return result;
}

static void ten_extension_context_add_extensions_info_from_graph(
    ten_extension_context_t *self, ten_list_t *extensions_info) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT(extensions_info, "Should not happen.");

  TEN_ASSERT(ten_list_size(&self->extensions_info_from_graph) == 0,
             "Should not happen.");

  ten_list_swap(&self->extensions_info_from_graph, extensions_info);
}

static void ten_extension_context_add_extension_groups_info_from_graph(
    ten_extension_context_t *self, ten_list_t *extension_groups_info) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  TEN_ASSERT(extension_groups_info, "Should not happen.");

  TEN_ASSERT(ten_list_size(&self->extension_groups_info_from_graph) == 0,
             "Should not happen.");

  ten_list_swap(&self->extension_groups_info_from_graph, extension_groups_info);
}

static void ten_extension_context_create_extension_group_done(
    ten_env_t *ten_env, ten_extension_group_t *extension_group) {
  TEN_ASSERT(extension_group &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The extension thread has not been created yet,
                 // so it is thread safe.
                 ten_extension_group_check_integrity(extension_group, false),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ENGINE,
             "Should not happen.");

  ten_engine_t *engine = ten_env_get_attached_engine(ten_env);
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_extension_context_t *extension_context = engine->extension_context;
  TEN_ASSERT(extension_context, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, true),
             "Invalid use of extension_context %p.", extension_context);

  ten_shared_ptr_t *original_start_graph_cmd =
      engine->original_start_graph_cmd_of_enabling_engine;
  TEN_ASSERT(original_start_graph_cmd, "Should not happen.");

  ten_addon_host_t *addon_host = extension_group->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");

  extension_group->app = engine->app;
  extension_group->extension_context = extension_context;

  if (ten_string_is_equal_c_str(&addon_host->name,
                                TEN_STR_DEFAULT_EXTENSION_GROUP)) {
    // default_extension_group is a special group, it needs the 'start_graph'
    // command to fill some important information.

    TEN_ASSERT(original_start_graph_cmd &&
                   ten_msg_get_type(original_start_graph_cmd) ==
                       TEN_MSG_TYPE_CMD_START_GRAPH &&
                   ten_msg_get_dest_cnt(original_start_graph_cmd) == 1,
               "Should not happen.");

    ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(original_start_graph_cmd);
    TEN_ASSERT(dest_loc, "Should not happen.");

    // Get the information of all the extensions which this extension group
    // should create.
    ten_list_t result =
        ten_cmd_start_graph_get_extension_addon_and_instance_name_pairs_of_specified_extension_group(
            original_start_graph_cmd,
            ten_string_get_raw_str(&dest_loc->app_uri),
            ten_string_get_raw_str(&dest_loc->graph_id),
            ten_string_get_raw_str(&extension_group->name));

    ten_list_swap(&extension_group->extension_addon_and_instance_name_pairs,
                  &result);
  }

  ten_extension_thread_t *extension_thread = ten_extension_thread_create();

  ten_extension_thread_attach_to_context_and_group(
      extension_thread, extension_context, extension_group);
  extension_group->extension_thread = extension_thread;

  ten_list_push_ptr_back(&extension_context->extension_threads,
                         extension_thread, NULL);

  size_t extension_groups_cnt_of_the_current_app = 0;

  ten_list_foreach (
      ten_cmd_start_graph_get_extension_groups_info(original_start_graph_cmd),
      iter) {
    ten_extension_group_info_t *extension_group_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));

    if (ten_string_is_equal(&extension_group_info->loc.app_uri,
                            &engine->app->uri)) {
      ++extension_groups_cnt_of_the_current_app;
    }
  }

  if (extension_groups_cnt_of_the_current_app ==
      ten_list_size(&extension_context->extension_threads)) {
    // All extension groups are created completed.

    ten_extension_context_add_extensions_info_from_graph(
        extension_context,
        ten_cmd_start_graph_get_extensions_info(original_start_graph_cmd));

    ten_extension_context_add_extension_groups_info_from_graph(
        extension_context, ten_cmd_start_graph_get_extension_groups_info(
                               original_start_graph_cmd));

    extension_group->extension_group_info =
        ten_extension_context_get_extension_group_info_by_name(
            extension_context, ten_app_get_uri(extension_context->engine->app),
            ten_extension_group_get_name(extension_group, true));
    TEN_ASSERT(extension_group->extension_group_info, "Should not happen.");

    ten_extension_context_start(extension_context);
  }
}

bool ten_extension_context_start_extension_group(ten_extension_context_t *self,
                                                 ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(self, true),
             "Invalid use of extension_context %p.", self);

  ten_engine_t *engine = self->engine;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_shared_ptr_t *original_start_graph_cmd =
      engine->original_start_graph_cmd_of_enabling_engine;
  TEN_ASSERT(original_start_graph_cmd &&
                 ten_msg_check_integrity(original_start_graph_cmd),
             "Should not happen.");

  bool result = true;

  ten_list_t *extension_info =
      ten_cmd_start_graph_get_extensions_info(original_start_graph_cmd);
  ten_list_t *extension_groups_info =
      ten_cmd_start_graph_get_extension_groups_info(original_start_graph_cmd);

  if (ten_list_is_empty(extension_groups_info)) {
    ten_extension_context_add_extensions_info_from_graph(self, extension_info);

    ten_extension_context_add_extension_groups_info_from_graph(
        self, extension_groups_info);

    ten_extension_context_start(self);

    goto done;
  }

  ten_env_t *ten_env = engine->ten_env;
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ENGINE,
             "Should not happen.");

  ten_list_foreach (extension_groups_info, iter) {
    ten_extension_group_info_t *extension_group_info =
        ten_shared_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
    TEN_ASSERT(extension_group_info, "Invalid argument.");
    TEN_ASSERT(ten_extension_group_info_check_integrity(extension_group_info),
               "Invalid use of extension_info %p.", extension_group_info);

    // Check whether the current `extension_group` is located within the current
    // `app`.
    if (ten_string_is_equal(&extension_group_info->loc.app_uri,
                            &self->engine->app->uri)) {
      bool res = ten_addon_create_extension_group(
          ten_env,
          ten_string_get_raw_str(
              &extension_group_info->extension_group_addon_name),
          ten_string_get_raw_str(
              &extension_group_info->loc.extension_group_name),
          (ten_env_addon_create_instance_done_cb_t)
              ten_extension_context_create_extension_group_done,
          NULL);

      if (!res) {
        TEN_LOGE(
            "[%s] Failed to start the extension group, because unable to find "
            "the specified extension group addon: %s",
            ten_engine_get_id(self->engine, true),
            ten_string_get_raw_str(
                &extension_group_info->extension_group_addon_name));

        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Unable to find %s",
                        ten_string_get_raw_str(
                            &extension_group_info->extension_group_addon_name));
        }

        result = false;
        break;
      }
    }
  }

done:
  return result;
}
