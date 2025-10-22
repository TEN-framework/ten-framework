//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/extension.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/base_dir.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/json.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "include_internal/ten_runtime/extension/msg_handling.h"
#include "include_internal/ten_runtime/extension/msg_not_connected_cnt.h"
#include "include_internal/ten_runtime/extension/on_xxx.h"
#include "include_internal/ten_runtime/extension/test.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/msg_interface/common.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path.h"
#include "include_internal/ten_runtime/path/path_group.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "include_internal/ten_runtime/path/result_return_policy.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/cmd/trigger_life_cycle/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/container/list_node_smart_ptr.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/container/list_smart_ptr.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"

static void ten_extension_handle_trigger_start_life_cycle(
    ten_extension_t *self, ten_shared_ptr_t *cmd);

static void ten_extension_handle_trigger_stop_life_cycle(ten_extension_t *self,
                                                         ten_shared_ptr_t *cmd);

ten_extension_t *ten_extension_create(
    const char *name, ten_extension_on_configure_func_t on_configure,
    ten_extension_on_init_func_t on_init,
    ten_extension_on_start_func_t on_start,
    ten_extension_on_stop_func_t on_stop,
    ten_extension_on_deinit_func_t on_deinit,
    ten_extension_on_cmd_func_t on_cmd, ten_extension_on_data_func_t on_data,
    ten_extension_on_audio_frame_func_t on_audio_frame,
    ten_extension_on_video_frame_func_t on_video_frame,
    TEN_UNUSED void *user_data) {
  TEN_ASSERT(name, "Should not happen.");

  ten_extension_t *self =
      (ten_extension_t *)TEN_MALLOC(sizeof(ten_extension_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_EXTENSION_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  // --------------------------
  // Public interface.
  self->on_configure = on_configure;
  self->on_init = on_init;
  self->on_start = on_start;
  self->on_stop = on_stop;
  self->on_deinit = on_deinit;
  self->on_cmd = on_cmd;
  self->on_data = on_data;
  self->on_audio_frame = on_audio_frame;
  self->on_video_frame = on_video_frame;

  // --------------------------
  // Private data.
  self->addon_host = NULL;
  ten_string_init_formatted(&self->name, "%s", name);

  self->ten_env = NULL;
  self->binding_handle.me_in_target_lang = self;
  self->extension_thread = NULL;
  self->extension_info = NULL;

  ten_list_init(&self->pending_msgs_received_before_on_init_done);
  ten_list_init(&self->path_timers);

  self->path_timeout_info.in_path_timeout = TEN_DEFAULT_PATH_TIMEOUT;
  self->path_timeout_info.out_path_timeout = TEN_DEFAULT_PATH_TIMEOUT;
  self->path_timeout_info.check_interval = TEN_DEFAULT_PATH_CHECK_INTERVAL;

  self->state = TEN_EXTENSION_STATE_INIT;

  ten_value_init_object_with_move(&self->manifest, NULL);
  ten_value_init_object_with_move(&self->property, NULL);
  ten_schema_store_init(&self->schema_store);

  self->manifest_info = NULL;
  self->property_info = NULL;

  self->path_table =
      ten_path_table_create(TEN_PATH_TABLE_ATTACH_TO_EXTENSION, self);

  self->ten_env = ten_env_create_for_extension(self);

  ten_hashtable_init(
      &self->msg_not_connected_count_map,
      offsetof(ten_extension_output_msg_not_connected_count_t, hh_in_map));

  self->is_standalone_test_extension = false;

  // Initialize manual trigger life cycle configuration
  for (int i = 0; i < TEN_MANUAL_TRIGGER_STAGE_MAX; i++) {
    self->manual_trigger_life_cycle.stages[i] = false;
  }

  ten_list_init(&self->pending_trigger_life_cycle_cmds);

  self->user_data = user_data;

  self->engine = NULL;
  self->app = NULL;

  return self;
}

bool ten_extension_check_integrity(ten_extension_t *self, bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_SIGNATURE) {
    return false;
  }

  ten_extension_thread_t *extension_thread = self->extension_thread;

  if (check_thread) {
    // Utilize the check_integrity of extension_thread to examine cases
    // involving the lock_mode of extension_thread.
    if (extension_thread) {
      if (ten_extension_thread_check_integrity_if_in_lock_mode(
              extension_thread)) {
        return true;
      }
    }
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

void ten_extension_destroy(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In TEN world, the destroy operations need to be performed in
  // any threads.
  TEN_ASSERT(ten_extension_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  TEN_ASSERT(self->ten_env, "Should not happen.");

  TEN_LOGI("[%s] Destroyed", ten_extension_get_name(self, false));

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);

  ten_env_destroy(self->ten_env);
  ten_string_deinit(&self->name);

  ten_value_deinit(&self->manifest);
  ten_value_deinit(&self->property);
  ten_schema_store_deinit(&self->schema_store);

  if (self->manifest_info) {
    ten_metadata_info_destroy(self->manifest_info);
    self->manifest_info = NULL;
  }
  if (self->property_info) {
    ten_metadata_info_destroy(self->property_info);
    self->property_info = NULL;
  }

  ten_list_clear(&self->pending_msgs_received_before_on_init_done);

  ten_path_table_check_empty(self->path_table);
  ten_path_table_destroy(self->path_table);

  TEN_ASSERT(ten_list_is_empty(&self->path_timers),
             "The path timers should all be closed before the destroy.");

  if (self->addon_host) {
    // Since the extension has already been destroyed, there is no need to
    // release its resources through the corresponding addon anymore. Therefore,
    // decrement the reference count of the corresponding addon.
    ten_ref_dec_ref(&self->addon_host->ref);
    self->addon_host = NULL;
  }

  ten_hashtable_deinit(&self->msg_not_connected_count_map);

  // Clean up pending trigger_life_cycle commands
  ten_list_clear(&self->pending_trigger_life_cycle_cmds);

  TEN_FREE(self);
}

static ten_msg_dest_info_t *ten_extension_get_msg_dests_from_graph_internal(
    ten_hashtable_t *dest_info_list, ten_shared_ptr_t *msg) {
  TEN_ASSERT(dest_info_list, "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");

  const char *msg_name = ten_msg_get_name(msg);
  TEN_ASSERT(msg_name, "Should not happen.");
  TEN_ASSERT(strlen(msg_name) > 0, "Should not happen.");

  ten_hashhandle_t *msg_dest_info_hh =
      ten_hashtable_find_string(dest_info_list, msg_name);
  if (msg_dest_info_hh) {
    ten_msg_dest_info_t *msg_dest = CONTAINER_OF_FROM_FIELD(
        msg_dest_info_hh, ten_msg_dest_info_t, hh_in_all_msg_type_dest_info);
    TEN_ASSERT(msg_dest, "Should not happen.");
    TEN_ASSERT(ten_msg_dest_info_check_integrity(msg_dest),
               "Should not happen.");

    return msg_dest;
  }

  return NULL;
}

static ten_msg_dest_info_t *ten_extension_get_msg_dests_from_graph(
    ten_extension_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");

  if (ten_msg_is_cmd_and_result(msg)) {
    return ten_extension_get_msg_dests_from_graph_internal(
        &self->extension_info->msg_dest_info.cmd, msg);
  } else {
    switch (ten_msg_get_type(msg)) {
    case TEN_MSG_TYPE_DATA:
      return ten_extension_get_msg_dests_from_graph_internal(
          &self->extension_info->msg_dest_info.data, msg);
    case TEN_MSG_TYPE_AUDIO_FRAME:
      return ten_extension_get_msg_dests_from_graph_internal(
          &self->extension_info->msg_dest_info.audio_frame, msg);
    case TEN_MSG_TYPE_VIDEO_FRAME:
      return ten_extension_get_msg_dests_from_graph_internal(
          &self->extension_info->msg_dest_info.video_frame, msg);
    default:
      TEN_ASSERT(0, "Should not happen.");
      return NULL;
    }
  }
}

static bool need_to_clone_msg_when_sending(ten_shared_ptr_t *msg,
                                           size_t dest_index) {
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  // Because when a message is sent to other extensions, these extensions
  // might be located in different extension groups. Therefore, after the
  // message is sent, if it is addressed to more than two extensions, the TEN
  // runtime needs to clone a copy of the message for extensions beyond the
  // first one, to avoid creating a thread safety issue. This principle applies
  // to all message types.
  //
  // A potential future optimization is that for the second and subsequent
  // extensions, if they are in the same extension group as the current
  // extension, then the cloning process can be omitted.

  return dest_index > 0;
}

static void ten_extension_determine_out_msg_dest_from_msg(
    ten_extension_t *self, ten_shared_ptr_t *msg, ten_list_t *result_msgs) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(ten_msg_get_dest_cnt(msg), "Invalid argument.");
  TEN_ASSERT(result_msgs, "Should not happen.");
  TEN_ASSERT(ten_list_size(result_msgs) == 0, "Should not happen.");

  ten_list_t dests = TEN_LIST_INIT_VAL;

  ten_list_swap(&dests, ten_msg_get_dest(msg));

  ten_list_foreach (&dests, iter) {
    ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(dest_loc, "Should not happen.");
    TEN_ASSERT(ten_loc_check_integrity(dest_loc), "Should not happen.");

    bool need_to_clone_msg = need_to_clone_msg_when_sending(msg, iter.index);

    ten_shared_ptr_t *curr_msg = NULL;
    if (need_to_clone_msg) {
      curr_msg = ten_msg_clone(msg, NULL);
    } else {
      curr_msg = msg;
    }

    ten_msg_clear_and_set_dest_to_loc(curr_msg, dest_loc);

    ten_adjust_msg_dest_for_standalone_test_scenario(curr_msg, self);

    ten_list_push_smart_ptr_back(result_msgs, curr_msg);

    if (need_to_clone_msg) {
      ten_shared_ptr_destroy(curr_msg);
    }
  }

  ten_list_clear(&dests);
}

/**
 * @return true if a destination is specified within the graph, false otherwise.
 */
static bool ten_extension_determine_out_msg_dest_from_graph(
    ten_extension_t *self, ten_shared_ptr_t *msg, ten_list_t *result_msgs,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(ten_msg_get_dest_cnt(msg) == 0, "Invalid argument.");
  TEN_ASSERT(result_msgs, "Invalid argument.");
  TEN_ASSERT(ten_list_size(result_msgs) == 0, "Invalid argument.");

  TEN_UNUSED ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  // Fetch the destinations from the graph.
  ten_msg_dest_info_t *msg_dest_info =
      ten_extension_get_msg_dests_from_graph(self, msg);
  if (msg_dest_info) {
    ten_list_t *dests = &msg_dest_info->dest;

    if (dests && ten_list_size(dests) > 0) {
      ten_list_foreach (dests, iter) {
        bool need_to_clone_msg =
            need_to_clone_msg_when_sending(msg, iter.index);

        ten_shared_ptr_t *curr_msg = NULL;
        if (need_to_clone_msg) {
          curr_msg = ten_msg_clone(msg, NULL);
        } else {
          curr_msg = msg;
        }

        ten_extension_info_t *dest_extension_info =
            ten_smart_ptr_get_data(ten_smart_ptr_listnode_get(iter.node));
        TEN_ASSERT(dest_extension_info, "Should not happen.");

        // TEN_NOLINTNEXTLINE(thread-check)
        // thread-check: The graph-related information of the extension remains
        // unchanged during the lifecycle of engine/graph, allowing safe
        // cross-thread access.
        TEN_ASSERT(
            ten_extension_info_check_integrity(dest_extension_info, false),
            "Invalid use of extension_info %p.", dest_extension_info);

        ten_msg_clear_and_set_dest_from_extension_info(curr_msg,
                                                       dest_extension_info);

        ten_adjust_msg_dest_for_standalone_test_scenario(curr_msg, self);

        ten_list_push_smart_ptr_back(result_msgs, curr_msg);

        if (need_to_clone_msg) {
          ten_shared_ptr_destroy(curr_msg);
        }
      }

      return true;
    }
  }

  // Graph doesn't specify how to route the messages.

  if (ten_add_msg_dest_for_standalone_test_scenario(msg, self)) {
    ten_list_push_smart_ptr_back(result_msgs, msg);
    return true;
  }

  TEN_MSG_TYPE msg_type = ten_msg_get_type(msg);
  const char *msg_name = ten_msg_get_name(msg);

  // In any case, the user needs to be informed about the error where the
  // graph does not have a specified destination for the message.
  TEN_ASSERT(err, "Should not happen.");
  ten_error_set(err, TEN_ERROR_CODE_MSG_NOT_CONNECTED,
                "Failed to find destination of a '%s' message '%s' from graph.",
                ten_msg_type_to_string(msg_type), msg_name);

  return false;
}

typedef enum TEN_EXTENSION_DETERMINE_OUT_MSGS_RESULT {
  TEN_EXTENSION_DETERMINE_OUT_MSGS_SUCCESS,
  TEN_EXTENSION_DETERMINE_OUT_MSGS_NOT_FOUND_IN_GRAPH,
} TEN_EXTENSION_DETERMINE_OUT_MSGS_RESULT;

/**
 * @brief The time to find the destination of a message is when that message
 * is going to leave an extension. There are 2 directions of sending
 * messages out of an extension:
 *
 *  msg <- (backward) <-
 *                      Extension
 *                                -> (forward) -> msg
 *
 * - Forward path:
 *   This is for all messages except the cmd results. The destination of
 * the message should be determined in the message itself, or in the graph.
 *
 * - Backward path:
 *   This is for cmd results only. The destination of the cmd result
 *   should be determined in the IN path table according to the command ID
 * of the cmd result.
 */
static TEN_EXTENSION_DETERMINE_OUT_MSGS_RESULT ten_extension_determine_out_msgs(
    ten_extension_t *self, ten_shared_ptr_t *msg, ten_list_t *result_msgs,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(result_msgs, "Invalid argument.");

  if (ten_msg_is_cmd_result(msg)) {
    // The destination loc of `cmd_result` will be refilled by the TEN runtime
    // using the information from the path_table. Therefore, when this function
    // receives the `cmd_result`, its destination loc must have values.
    TEN_ASSERT(ten_msg_get_dest_cnt(msg) > 0, "Should not happen.");
  }

  if (ten_msg_get_dest_cnt(msg) > 0) {
    // Because the messages has already had destinations, no matter it is a
    // backward path or a forward path, dispatch the message according to the
    // destinations specified in the message.

    ten_extension_determine_out_msg_dest_from_msg(self, msg, result_msgs);

    return TEN_EXTENSION_DETERMINE_OUT_MSGS_SUCCESS;
  } else {
    // Need to find the destinations from 2 databases:
    // - graph: all messages without the cmd results.
    // - IN path table: cmd results only.
    if (ten_extension_determine_out_msg_dest_from_graph(self, msg, result_msgs,
                                                        err)) {
      return TEN_EXTENSION_DETERMINE_OUT_MSGS_SUCCESS;
    } else {
      return TEN_EXTENSION_DETERMINE_OUT_MSGS_NOT_FOUND_IN_GRAPH;
    }
  }

  TEN_ASSERT(0, "Should not happen.");
  return TEN_EXTENSION_DETERMINE_OUT_MSGS_SUCCESS;
}

bool ten_extension_dispatch_msg(ten_extension_t *self, ten_shared_ptr_t *msg,
                                TEN_RESULT_RETURN_POLICY result_return_policy,
                                ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");
  TEN_ASSERT(err, "Invalid argument.");
  TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");

  if (self->state == TEN_EXTENSION_STATE_ON_DEINIT_DONE) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                    "The extension is closed, so the message cannot be "
                    "dispatched.");
    }
    return false;
  }

  bool result = true;

  bool msg_is_cmd_result = false;
  ten_list_t result_msgs = TEN_LIST_INIT_VAL;  // ten_shared_ptr_t*
  bool delete_msg = false;

  const bool msg_is_cmd = ten_msg_is_cmd_and_result(msg);

  if (ten_msg_get_type(msg) == TEN_MSG_TYPE_CMD_RESULT) {
    msg_is_cmd_result = true;

    // The backward path should strictly follow the information recorded
    // in the path table, therefore, users should not 'override' the
    // destination location in this case.
    ten_msg_clear_dest(msg);
  }

  // The source of the out message is the current extension.
  ten_msg_set_src_to_extension(msg, self);

  ten_msg_correct_dest(msg, ten_extension_get_belonging_engine(self));

  // The schema check for `msg` must be performed before message conversion and
  // path deletion for the following reasons:
  //
  // 1. The schema of `msg` sent or returned by the extension is defined by the
  // extension itself. However, message conversion may be defined within the
  // graph. Therefore, the schema check should be conducted before the
  // conversion, as the structure of `msg` may change after conversion.
  //
  // 2. For `cmd_result`, its path may be removed in
  // `ten_path_table_process_cmd_result()`. However, if the schema check fails,
  // it means that the `cmd_result` has failed to be sent. In this case, the
  // user can handle the failure and call `return_result()` again. Therefore, if
  // the schema check fails, the path should not be removed, which is a
  // reasonable approach.
  if (!ten_extension_validate_msg_schema(self, msg, true, err)) {
    result = false;
    goto done;
  }

  if (ten_msg_is_cmd_result(msg)) {
    ten_shared_ptr_t *processed_cmd_result = NULL;
    bool proceed = ten_path_table_process_cmd_result(
        self->path_table, TEN_PATH_IN, msg, &processed_cmd_result);
    if (!proceed) {
      // TEN_LOGD("[%s] IN path is missing, discard cmd_result",
      //          ten_extension_get_name(self, true));
      return true;
    } else {
      // TEN_LOGD("[%s] IN path is found, proceed cmd_result",
      //          ten_extension_get_name(self, true));

      if (msg != processed_cmd_result) {
        // It means `processed_cmd_result` is different from `msg`, so we need
        // to destroy the shared_ptr of the new `processed_cmd_result` after
        // processing it.
        msg = processed_cmd_result;
        delete_msg = true;
      }
    }
  }

  switch (ten_extension_determine_out_msgs(self, msg, &result_msgs, err)) {
  case TEN_EXTENSION_DETERMINE_OUT_MSGS_NOT_FOUND_IN_GRAPH:
    result = false;
    goto done;

  case TEN_EXTENSION_DETERMINE_OUT_MSGS_SUCCESS:
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (msg_is_cmd && !msg_is_cmd_result) {
    // Need to add all out path entries before _dispatching_ messages,
    // otherwise, the checking of receiving all results would be misjudgment.

    ten_list_t result_out_paths = TEN_LIST_INIT_VAL;

    ten_list_foreach (&result_msgs, iter) {
      ten_shared_ptr_t *result_msg = ten_smart_ptr_listnode_get(iter.node);
      TEN_ASSERT(result_msg, "Invalid argument.");
      TEN_ASSERT(ten_msg_check_integrity(result_msg), "Invalid argument.");

      ten_path_t *path = (ten_path_t *)ten_path_table_add_out_path(
          self->path_table, result_msg);
      TEN_ASSERT(path, "Should not happen.");
      TEN_ASSERT(ten_path_check_integrity(path, true), "Should not happen.");

      ten_list_push_ptr_back(&result_out_paths, path, NULL);
    }

    if (ten_list_size(&result_out_paths) > 1) {
      // Create a path group in this case.
      ten_paths_create_group(&result_out_paths, result_return_policy);
    }

    ten_list_clear(&result_out_paths);
  }

  // The handling of the OUT path table is completed, it's time to send the
  // message out of the extension.

  ten_list_foreach (&result_msgs, iter) {
    ten_shared_ptr_t *result_msg = ten_smart_ptr_listnode_get(iter.node);
    TEN_ASSERT(result_msg, "Invalid argument.");
    TEN_ASSERT(ten_msg_check_integrity(result_msg), "Invalid argument.");

    ten_extension_thread_dispatch_msg(self->extension_thread, result_msg);
  }

done:
  ten_list_clear(&result_msgs);
  if (delete_msg) {
    ten_shared_ptr_destroy(msg);
  }
  return result;
}

ten_runloop_t *ten_extension_get_attached_runloop(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads when
  // the extension is alive.
  TEN_ASSERT(ten_extension_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  return self->extension_thread->runloop;
}

static void ten_extension_on_configure(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_EXTENSION,
             "Invalid argument.");

  ten_extension_t *self = ten_env_get_attached_extension(ten_env);
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->state >= TEN_EXTENSION_STATE_PREPARE_TO_STOP) {
    // The extension has already entered the close flow, so do not continue with
    // the start flow.
    TEN_LOGI(
        "[%s] on_configure() skipped: Extension is already in the close flow",
        ten_extension_get_name(self, true));
    return;
  }

  TEN_LOGI("[%s] on_configure()", ten_extension_get_name(self, true));

  self->manifest_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_MANIFEST, self->ten_env);
  self->property_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_PROPERTY, self->ten_env);

  self->state = TEN_EXTENSION_STATE_ON_CONFIGURE;

  if (self->on_configure) {
    int64_t begin = ten_current_time_ms();

    self->on_configure(self, self->ten_env);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_configure() took %" PRId64 " ms",
               ten_extension_get_name(self, true), end - begin);
    }
  } else {
    ten_extension_on_configure_done(self->ten_env);
  }
}

void ten_extension_on_init(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->state >= TEN_EXTENSION_STATE_PREPARE_TO_STOP) {
    // The extension has already entered the close flow, so do not continue with
    // the start flow.
    TEN_LOGI("[%s] on_init() skipped: Extension is already in the close flow",
             ten_extension_get_name(self, true));
    return;
  }

  TEN_LOGI("[%s] on_init()", ten_extension_get_name(self, true));

  self->state = TEN_EXTENSION_STATE_ON_INIT;

  if (self->on_init) {
    int64_t begin = ten_current_time_ms();

    self->on_init(self, self->ten_env);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_init() took %" PRId64 " ms",
               ten_extension_get_name(self, true), end - begin);
    }
  } else {
    (void)ten_extension_on_init_done(self->ten_env);
  }
}

void ten_extension_on_start(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->state >= TEN_EXTENSION_STATE_PREPARE_TO_STOP) {
    // The extension has already entered the close flow, so do not continue with
    // the start flow.
    TEN_LOGI("[%s] on_start() skipped: Extension is already in the close flow",
             ten_extension_get_name(self, true));
    return;
  }

  TEN_LOGI("[%s] on_start()", ten_extension_get_name(self, true));

  self->state = TEN_EXTENSION_STATE_ON_START;

  if (self->on_start) {
    int64_t begin = ten_current_time_ms();

    self->on_start(self, self->ten_env);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_start() took %" PRId64 " ms",
               ten_extension_get_name(self, true), end - begin);
    }
  } else {
    ten_extension_on_start_done(self->ten_env);
  }
}

void ten_extension_on_stop(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  // There are two main starting points for triggering the `on_stop` process:
  //
  // 1. In `ten_extension_thread_stop_life_cycle_of_all_extensions`
  // 2. In `on_configure_done/on_init_done/on_start_done`
  //
  // Therefore, it is necessary to check here that if the `on_stop` process has
  // already started, it should not be entered again.
  if (self->state >= TEN_EXTENSION_STATE_ON_STOP) {
    TEN_LOGD("[%s] Extension is stopping, do not stop again",
             ten_extension_get_name(self, true));
    return;
  }

  TEN_LOGI("[%s] on_stop()", ten_extension_get_name(self, true));

  self->state = TEN_EXTENSION_STATE_ON_STOP;

  if (self->on_stop) {
    int64_t begin = ten_current_time_ms();

    self->on_stop(self, self->ten_env);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_stop() took %" PRId64 " ms",
               ten_extension_get_name(self, true), end - begin);
    }
  } else {
    ten_extension_on_stop_done(self->ten_env);
  }
}

void ten_extension_on_deinit(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  TEN_LOGI("[%s] on_deinit()", ten_extension_get_name(self, true));

  self->state = TEN_EXTENSION_STATE_ON_DEINIT;

  if (self->on_deinit) {
    int64_t begin = ten_current_time_ms();

    self->on_deinit(self, self->ten_env);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_deinit() took %" PRId64 " ms",
               ten_extension_get_name(self, true), end - begin);
    }
  } else {
    ten_extension_on_deinit_done(self->ten_env);
  }
}

void ten_extension_on_cmd(ten_extension_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  TEN_LOGD("[%s] on_cmd(%s)", ten_extension_get_name(self, true),
           ten_msg_get_name(msg));

  if (self->on_cmd) {
    int64_t begin = ten_current_time_ms();

    self->on_cmd(self, self->ten_env, msg);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_cmd(%s) took %" PRId64 " ms",
               ten_extension_get_name(self, true), ten_msg_get_name(msg),
               end - begin);
    }
  } else {
    // The default behavior of 'on_cmd' is to _not_ forward this command out,
    // and return an 'OK' result to the previous stage.
    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_OK, msg);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
  }
}

void ten_extension_on_data(ten_extension_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->on_data) {
    int64_t begin = ten_current_time_ms();

    self->on_data(self, self->ten_env, msg);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_data(%s) took %" PRId64 " ms",
               ten_extension_get_name(self, true), ten_msg_get_name(msg),
               end - begin);
    }
  } else {
    // Bypass the data.
    ten_env_send_data(self->ten_env, msg, NULL, NULL, NULL);
  }
}

void ten_extension_on_video_frame(ten_extension_t *self,
                                  ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->on_video_frame) {
    int64_t begin = ten_current_time_ms();

    self->on_video_frame(self, self->ten_env, msg);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_video_frame(%s) took %" PRId64 " ms",
               ten_extension_get_name(self, true), ten_msg_get_name(msg),
               end - begin);
    }
  } else {
    // Bypass the video frame.
    ten_env_send_video_frame(self->ten_env, msg, NULL, NULL, NULL);
  }
}

void ten_extension_on_audio_frame(ten_extension_t *self,
                                  ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  if (self->on_audio_frame) {
    int64_t begin = ten_current_time_ms();

    self->on_audio_frame(self, self->ten_env, msg);

    int64_t end = ten_current_time_ms();
    if (end - begin > TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS) {
      TEN_LOGW("[%s] on_audio_frame(%s) took %" PRId64 " ms",
               ten_extension_get_name(self, true), ten_msg_get_name(msg),
               end - begin);
    }
  } else {
    // Bypass the audio frame.
    ten_env_send_audio_frame(self->ten_env, msg, NULL, NULL, NULL);
  }
}

void ten_extension_load_metadata(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  TEN_LOGD("[%s] Load metadata", ten_extension_get_name(self, true));

  // This function is safe to be called from the extension main threads, because
  // all the resources it accesses are not be modified after the app
  // initialization phase.
  TEN_UNUSED ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  ten_metadata_load(ten_extension_on_configure, self->ten_env);
}

void ten_extension_set_me_in_target_lang(ten_extension_t *self,
                                         void *me_in_target_lang) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  self->binding_handle.me_in_target_lang = me_in_target_lang;
}

ten_env_t *ten_extension_get_ten_env(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: Need to pay attention to the thread safety of the using side
  // of this function.
  TEN_ASSERT(ten_extension_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  return self->ten_env;
}

ten_addon_host_t *ten_extension_get_addon(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  return self->addon_host;
}

const char *ten_extension_get_name(ten_extension_t *self, bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, check_thread),
             "Invalid use of extension %p.", self);

  return ten_string_get_raw_str(&self->name);
}

void ten_extension_set_addon(ten_extension_t *self,
                             ten_addon_host_t *addon_host) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, true),
             "Should not happen.");

  // Since the extension requires the corresponding addon to release
  // its resources, therefore, hold on to a reference count of the corresponding
  // addon.
  TEN_ASSERT(!self->addon_host, "Should not happen.");
  self->addon_host = addon_host;
  ten_ref_inc_ref(&addon_host->ref);
}

bool ten_extension_validate_msg_schema(ten_extension_t *self,
                                       ten_shared_ptr_t *msg, bool is_msg_out,
                                       ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  bool validated =
      ten_msg_validate_schema(msg, &self->schema_store, is_msg_out, err);
  if (!validated) {
    TEN_LOGW("[%s] See %s %s::%s with invalid schema: %s",
             ten_extension_get_name(self, true), is_msg_out ? "out" : "in",
             ten_msg_get_type_string(msg), ten_msg_get_name(msg),
             ten_error_message(err));

    if (!is_msg_out) {
      // Only when a schema checking error occurs before a message is sent to
      // the extension should an automatic generated error cmd result. This is
      // because, at this time, the extension has not had the opportunity to do
      // anything. Conversely, if a schema checking error occurs after a message
      // is sent out from the extension, the extension will be notified of the
      // error, giving it a chance to handle the error. Otherwise, if an
      // automatic error cmd result is also sent at this time, the act of
      // notifying the extension of a message error would be meaningless.

      switch (ten_msg_get_type(msg)) {
      case TEN_MSG_TYPE_CMD_TIMER:
      case TEN_MSG_TYPE_CMD_TIMEOUT:
      case TEN_MSG_TYPE_CMD_STOP_GRAPH:
      case TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE:
      case TEN_MSG_TYPE_CMD_CLOSE_APP:
      case TEN_MSG_TYPE_CMD_START_GRAPH:
      case TEN_MSG_TYPE_CMD: {
        ten_shared_ptr_t *cmd_result =
            ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, msg);
        ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                             ten_value_create_string(ten_error_message(err)),
                             NULL);
        ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
        ten_shared_ptr_destroy(cmd_result);
        break;
      }

      case TEN_MSG_TYPE_CMD_RESULT:
        // TODO(Liu): The detail or property in the cmd result might be
        // invalid, we should adjust the value according to the schema
        // definition. Ex: set the value to default after the schema system
        // supports `default` keyword.
        //
        // Set the status_code of the cmd result to an error code to notify to
        // the target extension that something wrong.
        //
        // TODO(Wei): Do we really need to set the status_code to error?
        ten_cmd_result_set_status_code(msg, TEN_STATUS_CODE_ERROR);

        // No matter what happens, the flow of the cmd result should
        // continue. Otherwise, the sender will not know what is happening,
        // and the entire command flow will be blocked.
        validated = true;
        break;

      case TEN_MSG_TYPE_DATA:
      case TEN_MSG_TYPE_VIDEO_FRAME:
      case TEN_MSG_TYPE_AUDIO_FRAME:
        // TODO(Liu): Provide a better way to let users know about this error
        // as there is no ack for msgs except cmd. We might consider dropping
        // this type of message at this point, and sending an event into the
        // extension.
        break;

      default:
        TEN_ASSERT(0, "Should not happen.");
        break;
      }
    }
  }

  return validated;
}

ten_extension_t *ten_extension_from_smart_ptr(
    ten_smart_ptr_t *extension_smart_ptr) {
  TEN_ASSERT(extension_smart_ptr, "Invalid argument.");
  return ten_smart_ptr_get_data(extension_smart_ptr);
}

void ten_extension_handle_trigger_life_cycle_cmd(ten_extension_t *self,
                                                 ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd, "Invalid argument.");
  TEN_ASSERT(ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE,
             "Invalid argument.");

  TEN_LOGD("[%s] Handle trigger_life_cycle command",
           ten_extension_get_name(self, true));

  const char *stage = ten_cmd_trigger_life_cycle_get_stage(cmd);
  if (!stage) {
    TEN_LOGW("[%s] trigger_life_cycle command missing stage field",
             ten_extension_get_name(self, true));

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                         ten_value_create_string("Missing stage field"), NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);

    return;
  }

  if (ten_c_string_is_equal(stage, TEN_STR_START)) {
    ten_extension_handle_trigger_start_life_cycle(self, cmd);
  } else if (ten_c_string_is_equal(stage, TEN_STR_STOP)) {
    ten_extension_handle_trigger_stop_life_cycle(self, cmd);
  } else {
    TEN_LOGW("[%s] trigger_life_cycle command with unsupported stage: %s",
             ten_extension_get_name(self, true), stage);

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(cmd_result, TEN_STR_DETAIL,
                         ten_value_create_string("Unsupported stage"), NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
  }
}

static void ten_extension_handle_trigger_start_life_cycle(
    ten_extension_t *self, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd, "Invalid argument.");

  TEN_LOGD("[%s] Handle trigger start life cycle",
           ten_extension_get_name(self, true));

  TEN_EXTENSION_STATE current_state = self->state;

  // Check if extension is configured for manual start control
  if (!self->manual_trigger_life_cycle.stages[TEN_MANUAL_TRIGGER_STAGE_START]) {
    TEN_LOGW(
        "[%s] trigger_life_cycle start command received but extension is not "
        "configured for manual start control",
        ten_extension_get_name(self, true));

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(
        cmd_result, TEN_STR_DETAIL,
        ten_value_create_string(
            "Extension not configured for manual start control"),
        NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
    return;
  }

  // Check if current state is valid for triggering start
  if (current_state > TEN_EXTENSION_STATE_ON_START_DONE) {
    TEN_LOGW(
        "[%s] trigger_life_cycle start command received but extension is "
        "already past start stage (state: %d)",
        ten_extension_get_name(self, true), current_state);

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(
        cmd_result, TEN_STR_DETAIL,
        ten_value_create_string("Extension already past start stage"), NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
    return;
  }

  // Store the command for later response
  ten_list_push_smart_ptr_back(&self->pending_trigger_life_cycle_cmds, cmd);

  // If extension is in ON_INIT_DONE state, trigger on_start immediately
  if (current_state == TEN_EXTENSION_STATE_ON_INIT_DONE) {
    TEN_LOGD("[%s] Extension is in ON_INIT_DONE state, triggering on_start",
             ten_extension_get_name(self, true));

    // Directly call on_start since we're already in the extension's runloop
    ten_extension_on_start(self);
  } else {
    TEN_LOGD(
        "[%s] Extension is in state %d, will trigger on_start after "
        "on_init_done",
        ten_extension_get_name(self, true), current_state);
  }
}

static void ten_extension_handle_trigger_stop_life_cycle(
    ten_extension_t *self, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd, "Invalid argument.");

  TEN_LOGD("[%s] Handle trigger stop life cycle",
           ten_extension_get_name(self, true));

  // Check if extension is configured for manual stop control
  if (!self->manual_trigger_life_cycle.stages[TEN_MANUAL_TRIGGER_STAGE_STOP]) {
    TEN_LOGW(
        "[%s] trigger_life_cycle stop command received but extension is not "
        "configured for manual stop control",
        ten_extension_get_name(self, true));

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(
        cmd_result, TEN_STR_DETAIL,
        ten_value_create_string(
            "Extension not configured for manual stop control"),
        NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
    return;
  }

  TEN_EXTENSION_STATE current_state = self->state;

  // Check if current state is valid for triggering stop
  if (current_state > TEN_EXTENSION_STATE_ON_STOP_DONE) {
    TEN_LOGW(
        "[%s] trigger_life_cycle stop command received but extension is "
        "already past stop stage (state: %d)",
        ten_extension_get_name(self, true), current_state);

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, cmd);
    ten_msg_set_property(
        cmd_result, TEN_STR_DETAIL,
        ten_value_create_string("Extension already past stop stage"), NULL);
    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
    return;
  }

  // Store the command for later response regardless of current state
  ten_list_push_smart_ptr_back(&self->pending_trigger_life_cycle_cmds, cmd);

  // Check if current state allows immediate triggering of stop
  if (current_state < TEN_EXTENSION_STATE_PREPARE_TO_STOP) {
    TEN_LOGD(
        "[%s] trigger_life_cycle stop command received before prepare to stop "
        "(state: %d), will trigger stop when appropriate",
        ten_extension_get_name(self, true), current_state);
    return;
  }

  // If extension is in TEN_EXTENSION_STATE_PREPARE_TO_STOP state, trigger
  // on_stop immediately
  if (current_state == TEN_EXTENSION_STATE_PREPARE_TO_STOP) {
    TEN_LOGD("[%s] Extension is in PREPARE_TO_STOP state, triggering on_stop",
             ten_extension_get_name(self, true));

    // Directly call on_stop since we're already in the extension's runloop
    ten_extension_on_stop(self);
  } else {
    TEN_LOGD(
        "[%s] Extension is in state %d, will trigger on_stop when appropriate",
        ten_extension_get_name(self, true), current_state);
  }
}

bool ten_extension_has_pending_trigger_life_cycle_cmds(ten_extension_t *self,
                                                       const char *stage) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(stage, "Invalid argument.");

  if (ten_list_is_empty(&self->pending_trigger_life_cycle_cmds)) {
    return false;
  }

  ten_list_foreach (&self->pending_trigger_life_cycle_cmds, iter) {
    ten_shared_ptr_t *cmd = ten_smart_ptr_listnode_get(iter.node);
    const char *cmd_stage = ten_cmd_trigger_life_cycle_get_stage(cmd);

    if (cmd_stage && ten_c_string_is_equal(cmd_stage, stage)) {
      return true;
    }
  }

  return false;
}

void ten_extension_reply_pending_trigger_life_cycle_cmds_by_stage(
    ten_extension_t *self, const char *stage, TEN_STATUS_CODE status_code) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(stage, "Invalid argument.");

  if (ten_list_is_empty(&self->pending_trigger_life_cycle_cmds)) {
    return;
  }

  TEN_LOGD(
      "[%s] Replying to pending trigger_life_cycle commands for stage '%s' "
      "with status %d",
      ten_extension_get_name(self, true), stage, status_code);

  // Use iterator to safely remove items while iterating
  ten_list_t commands_to_reply;
  ten_list_init(&commands_to_reply);

  ten_list_t commands_left;
  ten_list_init(&commands_left);

  // First, collect commands for the specified stage
  ten_list_foreach (&self->pending_trigger_life_cycle_cmds, iter) {
    ten_shared_ptr_t *cmd = ten_smart_ptr_listnode_get(iter.node);
    const char *cmd_stage = ten_cmd_trigger_life_cycle_get_stage(cmd);

    if (cmd_stage && ten_c_string_is_equal(cmd_stage, stage)) {
      ten_list_push_smart_ptr_back(&commands_to_reply, cmd);
    } else {
      ten_list_push_smart_ptr_back(&commands_left, cmd);
    }
  }

  // Replace the pending list with the commands left
  ten_list_swap(&self->pending_trigger_life_cycle_cmds, &commands_left);

  // Reply to the collected commands
  ten_list_foreach (&commands_to_reply, iter) {
    ten_shared_ptr_t *cmd = ten_smart_ptr_listnode_get(iter.node);

    ten_shared_ptr_t *cmd_result =
        ten_cmd_result_create_from_cmd(status_code, cmd);

    if (status_code == TEN_STATUS_CODE_OK) {
      ten_msg_set_property(
          cmd_result, TEN_STR_DETAIL,
          ten_value_create_string("Life cycle triggered successfully"), NULL);
    }

    ten_env_return_result(self->ten_env, cmd_result, NULL, NULL, NULL);
    ten_shared_ptr_destroy(cmd_result);
  }

  // Clean up the temporary list
  ten_list_clear(&commands_to_reply);
  ten_list_clear(&commands_left);
}

void ten_extension_trigger_stop_if_needed(ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");

  self->state = TEN_EXTENSION_STATE_PREPARE_TO_STOP;

  // Check if extension is configured for manual stop control
  if (!self->manual_trigger_life_cycle.stages[TEN_MANUAL_TRIGGER_STAGE_STOP]) {
    // Not configured for manual control, trigger stop directly
    TEN_LOGD(
        "[%s] Extension not configured for manual stop control, triggering "
        "stop directly",
        ten_extension_get_name(self, true));
    ten_extension_on_stop(self);
    return;
  }

  // Extension is configured for manual stop control
  // Check if there are pending trigger_life_cycle stop commands
  if (ten_extension_has_pending_trigger_life_cycle_cmds(self, TEN_STR_STOP)) {
    TEN_LOGD(
        "[%s] Extension configured for manual stop control and has pending "
        "stop trigger commands, triggering stop",
        ten_extension_get_name(self, true));
    ten_extension_on_stop(self);
  } else {
    TEN_LOGD(
        "[%s] Extension configured for manual stop control, waiting for stop "
        "trigger command",
        ten_extension_get_name(self, true));
  }
}
