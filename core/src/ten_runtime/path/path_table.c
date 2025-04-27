//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/path/path_table.h"

#include <inttypes.h>

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/base.h"
#include "include_internal/ten_runtime/path/common.h"
#include "include_internal/ten_runtime/path/path.h"
#include "include_internal/ten_runtime/path/path_group.h"
#include "include_internal/ten_runtime/path/path_in.h"
#include "include_internal/ten_runtime/path/path_out.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/sanitizer/thread_check.h"

#define PATH_TABLE_REASONABLE_MAX_CNT 1000

static bool ten_path_table_check_integrity(ten_path_table_t *self,
                                           bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_PATH_TABLE_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    if (self->attach_to == TEN_PATH_TABLE_ATTACH_TO_EXTENSION) {
      ten_extension_thread_t *extension_thread =
          self->attached_target.extension->extension_thread;
      return ten_extension_thread_check_integrity(extension_thread, true);
    }

    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

ten_path_table_t *ten_path_table_create(TEN_PATH_TABLE_ATTACH_TO attach_to,
                                        void *attached_target) {
  ten_path_table_t *self =
      (ten_path_table_t *)TEN_MALLOC(sizeof(ten_path_table_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_PATH_TABLE_SIGNATURE);

  ten_list_init(&self->in_paths);
  ten_list_init(&self->out_paths);

  self->attach_to = attach_to;
  switch (attach_to) {
  case TEN_PATH_TABLE_ATTACH_TO_ENGINE:
    self->attached_target.engine = attached_target;
    break;
  case TEN_PATH_TABLE_ATTACH_TO_EXTENSION:
    self->attached_target.extension = attached_target;
    break;
  case TEN_PATH_TABLE_ATTACH_TO_APP:
    self->attached_target.app = attached_target;
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  return self;
}

void ten_path_table_destroy(ten_path_table_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function would be called from the engine thread, so we
  // do not check the execution thread context of the path table.
  TEN_ASSERT(self && ten_path_table_check_integrity(self, false),
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_list_clear(&self->in_paths);
  ten_list_clear(&self->out_paths);

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

void ten_path_table_check_empty(ten_path_table_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function would be called when extensions are destroying,
  // So at that time, the extension thread had already been deleted. Therefore,
  // this function is invoked within the engine thread and is safe.
  TEN_ASSERT(self && ten_path_table_check_integrity(self, false),
             "Should not happen.");

  TEN_ASSERT(ten_list_is_empty(&self->in_paths), "There should be no IN path.");
  TEN_ASSERT(ten_list_is_empty(&self->out_paths),
             "There should be no OUT path.");
}

static ten_listnode_t *ten_path_table_find_path_from_cmd_id(
    ten_path_table_t *self, TEN_PATH_TYPE type, const char *cmd_id) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(cmd_id, "Invalid argument.");

  ten_list_t *list = type == TEN_PATH_IN ? &self->in_paths : &self->out_paths;

  size_t path_cnt = ten_list_size(list);

  // Some simple sanity check.
  if (path_cnt > PATH_TABLE_REASONABLE_MAX_CNT) {
    TEN_LOGE("Too many paths, there might be some issues.");
  }

  ten_list_foreach (list, paths_iter) {
    ten_path_t *path = (ten_path_t *)ten_ptr_listnode_get(paths_iter.node);
    TEN_ASSERT(path && ten_path_check_integrity(path, true),
               "Should not happen.");

    // TEN_LOGD("path(%s) cmd_id: %s, cmd_id: %s",
    //          type == TEN_PATH_IN ? "in" : "out",
    //          ten_string_get_raw_str(&path->cmd_id), cmd_id);

    if (ten_string_is_equal_c_str(&path->cmd_id, cmd_id)) {
      return paths_iter.node;
    }
  }

  return NULL;
}

static uint64_t get_expired_time(uint64_t timeout_duration) {
  int64_t current_time_us_ = ten_current_time_us();
  TEN_ASSERT(current_time_us_ >= 0, "Should not happen. %" PRId64,
             current_time_us_);

  uint64_t current_time_us = current_time_us_;
  if (timeout_duration > UINT64_MAX - current_time_us) {
    return UINT64_MAX;
  }

  return current_time_us + timeout_duration;
}

static uint64_t ten_path_table_get_path_timeout_duration(
    ten_path_table_t *self, TEN_PATH_TYPE path_type) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Invalid argument.");

  uint64_t timeout_time = UINT64_MAX;

  if (self->attach_to == TEN_PATH_TABLE_ATTACH_TO_EXTENSION) {
    ten_extension_t *extension = self->attached_target.extension;
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid argument.");

    // Try to get general path timeout time in us from the extension.
    switch (path_type) {
    case TEN_PATH_IN:
      timeout_time = extension->path_timeout_info.in_path_timeout;
      break;
    case TEN_PATH_OUT:
      timeout_time = extension->path_timeout_info.out_path_timeout;
      break;
    default:
      break;
    }
  }

  return timeout_time;
}

/**
 * --> Extension
 *  ^
 *
 * TEN records this kind of path to determine where the messages (ex: the status
 * commands) should go when they follow the backward path.
 */
ten_path_in_t *ten_path_table_add_in_path(
    ten_path_table_t *self, ten_shared_ptr_t *cmd,
    ten_msg_conversion_t *result_conversion) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd) &&
                 ten_msg_get_dest_cnt(cmd) == 1,
             "Should not happen.");

  ten_listnode_t *old_node = ten_path_table_find_path_from_cmd_id(
      self, TEN_PATH_IN, ten_cmd_base_get_cmd_id(cmd));
  if (old_node) {
    // The presence of the command's path in the path table indicates a
    // potential circular reference, exemplified by the following structure:
    //
    // A --> B --> C
    //       ^     |
    //       |     V
    //       <---- D
    //
    // Such a circular path scenario necessitates the generation of a unique
    // command ID for this specific command. This approach helps prevent path
    // conflicts and ensures accurate identification of the correct path entry
    // in the table. This is essential for maintaining the integrity of path
    // tracking and avoiding erroneous command executions or data processing.
    if (ten_msg_is_cmd(cmd)) {
      ten_cmd_base_save_cmd_id_to_parent_cmd_id(cmd);

      ten_cmd_base_gen_new_cmd_id_forcibly(cmd);

      old_node = ten_path_table_find_path_from_cmd_id(
          self, TEN_PATH_IN, ten_cmd_base_get_cmd_id(cmd));
    }
  }

  TEN_ASSERT(!old_node,
             "There should be no two commands with the same command ID.");

  ten_loc_t src_loc;
  ten_loc_init_from_loc(&src_loc, ten_msg_get_src_loc(cmd));

  ten_path_in_t *in_path = ten_path_in_create(
      self, ten_msg_get_name(cmd), ten_cmd_base_get_parent_cmd_id(cmd),
      ten_cmd_base_get_cmd_id(cmd), &src_loc, result_conversion);
  TEN_ASSERT(in_path && ten_path_check_integrity((ten_path_t *)in_path, true),
             "Invalid argument.");

  uint64_t timeout_duration_us =
      ten_path_table_get_path_timeout_duration(self, TEN_PATH_IN);
  ten_path_set_expired_time(&in_path->base,
                            get_expired_time(timeout_duration_us));

  // The parent_cmd_id has been saved to the path entry, so clear it from the
  // command itself.
  ten_cmd_base_reset_parent_cmd_id(cmd);

  ten_list_push_ptr_back(&self->in_paths, in_path,
                         (ten_ptr_listnode_destroy_func_t)ten_path_in_destroy);

  ten_loc_deinit(&src_loc);

  return in_path;
}

/**
 * Extension -->
 *       ^
 *
 * TEN records this kind of path to enable the cmd result to get some
 * original information, ex: the result handler.
 */
ten_path_out_t *ten_path_table_add_out_path(ten_path_table_t *self,
                                            ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd) &&
                 ten_msg_get_dest_cnt(cmd) == 1,
             "Should not happen.");

  ten_loc_t src_loc;
  ten_loc_init_from_loc(&src_loc, ten_msg_get_src_loc(cmd));

  // Create a path.
  ten_path_out_t *out_path = ten_path_out_create(
      self, ten_msg_get_name(cmd), ten_cmd_base_get_parent_cmd_id(cmd),
      ten_cmd_base_get_cmd_id(cmd), &src_loc,
      ten_cmd_base_get_raw_cmd_base(cmd)->result_handler,
      ten_cmd_base_get_raw_cmd_base(cmd)->result_handler_data);
  TEN_ASSERT(out_path && ten_path_check_integrity((ten_path_t *)out_path, true),
             "Invalid argument.");

  uint64_t timeout_duration_us =
      ten_path_table_get_path_timeout_duration(self, TEN_PATH_OUT);
  ten_path_set_expired_time(&out_path->base,
                            get_expired_time(timeout_duration_us));

  // The parent_cmd_id has been saved to the path entry, so clear it from the
  // command itself.
  ten_cmd_base_reset_parent_cmd_id(cmd);

  // Reset the result handler after saving it into the path.
  ten_cmd_base_set_result_handler(cmd, NULL, NULL);

  // Save the created path to the path table.
  ten_list_push_ptr_back(&self->out_paths, out_path,
                         (ten_ptr_listnode_destroy_func_t)ten_path_out_destroy);

  ten_loc_deinit(&src_loc);

  return out_path;
}

static ten_listnode_t *ten_path_table_find_path_from_cmd(
    ten_path_table_t *self, TEN_PATH_TYPE path_type, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");

  ten_listnode_t *path_node = ten_path_table_find_path_from_cmd_id(
      self, path_type, ten_cmd_base_get_cmd_id(cmd));
  if (!path_node) {
    return NULL;
  }

  return path_node;
}

static ten_listnode_t *ten_path_table_find_out_path(ten_path_table_t *self,
                                                    ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");

  ten_listnode_t *path_node =
      ten_path_table_find_path_from_cmd(self, TEN_PATH_OUT, cmd);
  if (!path_node) {
    return NULL;
  }

  return path_node;
}

/**
 * @brief TEN finds this kind of path to determine where the messages (ex: the
 * cmd results) should go when they are returning.
 */
static ten_listnode_t *ten_path_table_find_in_path(ten_path_table_t *self,
                                                   ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_path_table_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Invalid argument.");

  // Find the corresponding IN path according to the "command ID".
  ten_listnode_t *path_node =
      ten_path_table_find_path_from_cmd(self, TEN_PATH_IN, cmd);
  if (!path_node) {
    return NULL;
  }

  return path_node;
}

static void ten_path_table_remove_group_and_all_its_paths(
    ten_path_table_t *self, TEN_PATH_TYPE type, ten_path_group_t *path_group) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_path_table_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(path_group && ten_path_group_check_integrity(path_group, true),
             "Invalid argument.");

  ten_list_t *paths = type == TEN_PATH_IN ? &self->in_paths : &self->out_paths;

  ten_list_t *group_members = ten_path_group_get_members(path_group);

  ten_list_foreach (group_members, iter) {
    ten_path_t *group_path = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(group_path && ten_path_check_integrity(group_path, true),
               "Invalid argument.");

    ten_listnode_t *group_path_node = ten_list_find_ptr(paths, group_path);
    TEN_ASSERT(group_path_node, "Should not happen.");

    ten_list_remove_node(paths, group_path_node);
  }
}

// Search the specified path table for the corresponding path entry.
static ten_listnode_t *ten_path_table_find_path(ten_path_table_t *self,
                                                TEN_PATH_TYPE path_type,
                                                ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Invalid argument.");

  switch (path_type) {
  case TEN_PATH_IN:
    return ten_path_table_find_in_path(self, cmd);
  case TEN_PATH_OUT:
    return ten_path_table_find_out_path(self, cmd);
  default:
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

static void ten_path_table_remove_path(ten_path_table_t *self,
                                       TEN_PATH_TYPE path_type,
                                       ten_listnode_t *path_node) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_path_table_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(path_node && ten_listnode_check_integrity(path_node),
             "Invalid argument.");

  ten_list_t *paths =
      path_type == TEN_PATH_IN ? &self->in_paths : &self->out_paths;

  ten_list_remove_node(paths, path_node);
}

static void ten_cmd_result_set_info_from_path(ten_shared_ptr_t *cmd_result,
                                              TEN_PATH_TYPE path_type,
                                              ten_path_t *path) {
  TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
             "Invalid argument.");
  TEN_ASSERT(path && ten_path_check_integrity(path, true), "Invalid argument.");

  ten_cmd_result_set_original_cmd_name(cmd_result,
                                       ten_string_get_raw_str(&path->cmd_name));

  // The command ID should be reverted to an old one when flows through this
  // path.
  if (!ten_string_is_empty(&path->parent_cmd_id)) {
    ten_cmd_base_set_cmd_id(cmd_result,
                            ten_string_get_raw_str(&path->parent_cmd_id));
  }

  // Set the dest of the cmd result to the source location of the path.
  ten_msg_clear_and_set_dest_to_loc(cmd_result, &path->src_loc);

  if (path_type == TEN_PATH_OUT) {
    // Restore the settings of the result handler, so that the extension
    // could call the result handler for the result.
    ten_cmd_base_set_result_handler(
        cmd_result, ((ten_path_out_t *)path)->result_handler,
        ((ten_path_out_t *)path)->result_handler_data);
  }
}

// There are 2 cases where a cmd result interacted with an extension.
//
//              (1)                                     (2)
//                                            ↙-- cmd_result (a path)
// <-o<-- cmd_result (a path) <- Extension <-o<-- cmd_result (a path)
//                                            ↖-- cmd_result (a path)
//
// (1) When a cmd result leaves an extension
//     The cmd result would flow through a _IN_ path of the current extension.
//
// (2) When a cmd result enters an extension
//     Multiple cmd results might be related to a single original command due to
//     the graph 'dests' dispatching mechanism. Each cmd result would flow
//     through a _OUT_ path of the current extension.
bool ten_path_table_process_cmd_result(
    ten_path_table_t *self, TEN_PATH_TYPE path_type,
    ten_shared_ptr_t *cmd_result, ten_shared_ptr_t **processed_cmd_result) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(cmd_result &&
                 ten_msg_get_type(cmd_result) == TEN_MSG_TYPE_CMD_RESULT &&
                 ten_cmd_base_check_integrity(cmd_result),
             "Invalid argument.");
  TEN_ASSERT(processed_cmd_result, "Invalid argument.");

  bool proceed = true;

  ten_listnode_t *path_node =
      ten_path_table_find_path(self, path_type, cmd_result);
  if (!path_node) {
    // The path for the cmd_result to return is no longer available.
    // TEN_LOGI("The path for the cmd_result to return is no longer
    // available.");

    proceed = false;
    goto done;
  }

  ten_path_t *path = ten_ptr_listnode_get(path_node);
  TEN_ASSERT(path && ten_path_check_integrity(path, true), "Invalid argument.");

  // Since a `cmd_result` being considered completed for one path group does not
  // necessarily mean it is also completed for previous path groups, the
  // completed attribute is reset each time to ensure that the completion status
  // is re-evaluated.
  ten_cmd_result_set_completed(cmd_result, false, NULL);

  if (ten_cmd_result_is_final(cmd_result, NULL)) {
    path->has_received_final_cmd_result = true;
  }

  if (path->result_conversion) {
    // If there is a cmd_result conversion setting, use it.

    // Currently, only the PATH_IN route has the result conversion mechanism.
    // This means that result conversion only occurs on the route before the
    // command reaches the destination extension. However, on the route where
    // the command is sent from an extension, there is no result conversion
    // mechanism.
    TEN_ASSERT(path->type == TEN_PATH_IN, "Invalid argument.");

    ten_error_t err;
    TEN_ERROR_INIT(err);

    ten_shared_ptr_t *converted_cmd_result =
        ten_msg_conversion_convert(path->result_conversion, cmd_result, &err);
    if (!converted_cmd_result) {
      TEN_LOGE("Failed to convert cmd result: %s", ten_error_message(&err));

      // Since the flow of the cmd_result must not be interrupted, otherwise
      // the extension, which expects to receive the cmd_result, will not
      // receive it and will hang. Therefore, if converting the cmd_result
      // fails here, there will be a fallback path, which is to clone the
      // original cmd_result, allowing the extension to receive 'what may be
      // an incorrect cmd_result'. This at least ensures that the cmd_result is
      // recognized as incorrect, enabling users to check which part of the
      // conversion rule is problematic.
      ten_cmd_result_set_status_code(cmd_result, TEN_STATUS_CODE_ERROR);
    } else {
      cmd_result = converted_cmd_result;
    }

    ten_error_deinit(&err);
  }

  if (path->group) {
    // The one-to-many situation only occurs on the route where a message is
    // sent from an extension, forming a group. Apart from this, the number of
    // destinations for a message within the TEN runtime should always be
    // one.
    TEN_ASSERT(path->type == TEN_PATH_OUT, "Invalid argument.");

    ten_path_group_t *path_group = ten_path_get_group(path);
    TEN_ASSERT(path_group && ten_path_group_check_integrity(path_group, true),
               "Invalid argument.");
    TEN_ASSERT(path_group->policy != TEN_RESULT_RETURN_POLICY_INVALID,
               "Invalid argument.");

    switch (path_group->policy) {
    case TEN_RESULT_RETURN_POLICY_FIRST_ERROR_OR_LAST_OK:
      if (ten_cmd_result_get_status_code(cmd_result) != TEN_STATUS_CODE_OK) {
        proceed = true;
        ten_cmd_result_set_info_from_path(cmd_result, path_type, path);

        // This path group has completed its task. Cancel the entire path
        // group and all associated paths, and let this failed `cmd_result`
        // flow back.
        ten_path_table_remove_group_and_all_its_paths(self, path_type,
                                                      path_group);

        ten_cmd_result_set_completed(cmd_result, true, NULL);
      } else {
        if (path->last_in_group) {
          ten_cmd_result_set_info_from_path(cmd_result, path_type, path);
          path->cached_cmd_result = ten_shared_ptr_clone(cmd_result);
        }

        ten_list_t *members = &path_group->members;
        TEN_ASSERT(members && ten_list_check_integrity(members),
                   "Should not happen.");

        bool received_all_final_results = true;
        ten_shared_ptr_t *cached_cmd_result = NULL;

        ten_list_foreach (members, iter) {
          ten_path_t *path = ten_ptr_listnode_get(iter.node);
          TEN_ASSERT(path && ten_path_check_integrity(path, true),
                     "Invalid argument.");

          cached_cmd_result = path->cached_cmd_result;

          if (!path->has_received_final_cmd_result) {
            received_all_final_results = false;
            break;
          }
        }

        if (!received_all_final_results) {
          // Not all `cmd_results` have been received yet, so do not proceed
          // with the process of flowing back `cmd_result` for now.
          proceed = false;
        } else {
          TEN_ASSERT(cached_cmd_result &&
                         ten_cmd_base_check_integrity(cached_cmd_result),
                     "Should not happen.");

          cmd_result = ten_shared_ptr_clone(cached_cmd_result);

          proceed = true;
          ten_cmd_result_set_info_from_path(cmd_result, path_type, path);

          ten_cmd_result_set_completed(cmd_result, true, NULL);

          ten_path_table_remove_group_and_all_its_paths(self, path_type,
                                                        path_group);
        }
      }
      break;

    case TEN_RESULT_RETURN_POLICY_EACH_OK_AND_ERROR: {
      // Check whether we need to mark `cmd_result` as completed.
      ten_list_t *members = &path_group->members;
      TEN_ASSERT(members && ten_list_check_integrity(members),
                 "Should not happen.");

      bool received_all_final_results = true;

      ten_list_foreach (members, iter) {
        ten_path_t *path = ten_ptr_listnode_get(iter.node);
        TEN_ASSERT(path && ten_path_check_integrity(path, true),
                   "Invalid argument.");

        if (!path->has_received_final_cmd_result) {
          received_all_final_results = false;
          break;
        }
      }

      proceed = true;
      ten_cmd_result_set_info_from_path(cmd_result, path_type, path);

      if (received_all_final_results) {
        ten_cmd_result_set_completed(cmd_result, true, NULL);

        ten_path_table_remove_group_and_all_its_paths(self, path_type,
                                                      path_group);
      }
      break;
    }
    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
  } else {
    proceed = true;
    ten_cmd_result_set_info_from_path(cmd_result, path_type, path);

    if (ten_cmd_result_is_final(cmd_result, NULL)) {
      ten_cmd_result_set_completed(cmd_result, true, NULL);

      ten_path_table_remove_path(self, path_type, path_node);
      path_node = NULL;
    }
  }

done:
  if (proceed) {
    *processed_cmd_result = cmd_result;
  } else {
    *processed_cmd_result = NULL;
  }
  return proceed;
}

ten_string_t *ten_path_table_get_graph_id(ten_path_table_t *self) {
  TEN_ASSERT(self && ten_path_table_check_integrity(self, true),
             "Invalid argument.");
  if (self->attach_to == TEN_PATH_TABLE_ATTACH_TO_EXTENSION) {
    ten_extension_context_t *extension_context =
        self->attached_target.extension->extension_context;
    TEN_ASSERT(
        extension_context &&
            // TEN_NOLINTNEXTLINE(thread-check)
            // thread-check: We are in the extension thread, and all the uses of
            // the extension context in this function would not cause thread
            // safety issues.
            ten_extension_context_check_integrity(extension_context, false),
        "Invalid argument.");

    ten_engine_t *engine = extension_context->engine;
    TEN_ASSERT(
        engine &&
            // TEN_NOLINTNEXTLINE(thread-check)
            // thread-check: We are in the extension thread, and all the uses of
            // the engine in this function would not cause thread safety issues.
            ten_engine_check_integrity(engine, false),
        "Invalid argument.");

    return &engine->graph_id;
  } else {
    ten_engine_t *engine = self->attached_target.engine;
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
               "Invalid argument.");
    return &engine->graph_id;
  }
}
