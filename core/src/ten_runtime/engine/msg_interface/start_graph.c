//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/msg_interface/start_graph.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/close.h"
#include "include_internal/ten_runtime/engine/internal/extension_interface.h"
#include "include_internal/ten_runtime/engine/internal/remote_interface.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/start_graph/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path.h"
#include "include_internal/ten_runtime/path/path_group.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

void ten_engine_handle_cmd_start_graph(ten_engine_t *self,
                                       ten_shared_ptr_t *cmd,
                                       ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(cmd, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true),
             "The usage of the engine is incorrect.");
  TEN_ASSERT(
      ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_START_GRAPH,
      "The command this function handles should be a 'start_graph' command.");
  TEN_ASSERT(ten_msg_get_src_app_uri(cmd),
             "The 'start_graph' command should have a src_uri information.");

  ten_cmd_start_graph_add_missing_extension_group_node(cmd);

  ten_list_t immediate_connectable_apps = TEN_LIST_INIT_VAL;
  ten_cmd_start_graph_collect_all_immediate_connectable_apps(
      cmd, self->app, &immediate_connectable_apps);

  // It cannot set `original_start_graph_cmd_of_enabling_engine` here directly
  // because if the `engine` was started due to a previously received
  // `start_graph` command, setting
  // `original_start_graph_cmd_of_enabling_engine` upon receiving a new
  // `start_graph` command here could result in the engine responding to the
  // wrong recipient after a successful startup.

  if (ten_list_is_empty(&immediate_connectable_apps)) {
    // This case will be triggered when the graph involves only a single app.

    TEN_LOGD(
        "No more extensions need to be connected in the graph, enable the "
        "extension system now.");

    // Starting the extension system is an asynchronous action, so the
    // `start_graph` command that triggers this action needs to be saved first.
    // This ensures that once the startup is successful, a response can be sent
    // to the correct `start_graph` command.
    TEN_ASSERT(!self->original_start_graph_cmd_of_enabling_engine,
               "Should not happen.");
    self->original_start_graph_cmd_of_enabling_engine =
        ten_shared_ptr_clone(cmd);

    ten_engine_enable_extension_system(self, err);
  } else {
    // There are more apps need to be connected, so connect them now.
    ten_list_t new_works = TEN_LIST_INIT_VAL;
    bool error_occurred = false;

    ten_list_foreach (&immediate_connectable_apps, iter) {
      ten_string_t *dest_uri = ten_str_listnode_get(iter.node);
      TEN_ASSERT(dest_uri, "Invalid argument.");

      const char *dest_uri_c_str = ten_string_get_raw_str(dest_uri);
      TEN_ASSERT(dest_uri_c_str, "Invalid argument.");

      TEN_LOGD("Check if we have connected to %s.", dest_uri_c_str);

      // Check to see if we have connected to this URI or not.
      ten_remote_t *remote =
          ten_engine_check_remote_is_existed(self, dest_uri_c_str);
      if (!remote) {
        TEN_LOGD("%s is unconnected, connect now.", dest_uri_c_str);

        ten_shared_ptr_t *child_cmd = ten_msg_clone(cmd, NULL);
        TEN_ASSERT(child_cmd, "Should not happen.");

        // The remote app does not recognize the local app's
        // 'predefined_graph_name', so this field should not be included in the
        // 'start_graph' command which will be sent to the remote app.
        ten_cmd_start_graph_set_predefined_graph_name(child_cmd, "", err);

        // Use the uri of the local app to fill/override the value of 'from'
        // field (even if there is any old value in the 'from' field), so that
        // the remote could know who connects to them.
        ten_msg_set_src_to_engine(child_cmd, self);

        // Correct the destination information of the 'start_graph' command.
        ten_msg_clear_and_set_dest(child_cmd, dest_uri_c_str,
                                   ten_string_get_raw_str(&self->graph_id),
                                   NULL, NULL, err);

        ten_path_t *out_path = (ten_path_t *)ten_path_table_add_out_path(
            self->path_table, child_cmd);
        TEN_ASSERT(out_path && ten_path_check_integrity(out_path, true),
                   "Should not happen.");

        ten_list_push_ptr_back(&new_works, out_path, NULL);

        bool rc = ten_engine_connect_to_graph_remote(
            self, ten_string_get_raw_str(dest_uri), child_cmd);
        if (!rc) {
          TEN_LOGE("[%s] Failed to connect to %s.", ten_app_get_uri(self->app),
                   dest_uri_c_str);

          error_occurred = true;
          ten_shared_ptr_destroy(child_cmd);

          ten_engine_return_error_for_cmd_start_graph(
              self, cmd, "Failed to connect to %s.", dest_uri_c_str);
          break;
        }
      } else {
        TEN_LOGD("%s is connected, there is nothing to do.", dest_uri_c_str);
      }
    }

    if (error_occurred) {
      // An error occurred, so we should not continue to connect to the
      // remaining apps (remotes).
      ten_list_clear(&new_works);
    } else {
      if (ten_list_is_empty(&new_works) == false) {
        // This means that we can _not_ start the engine now. We must wait for
        // these newly submitted 'start_graph' commands to be completed in order
        // to start the engine, and this is an asynchronous action, so we must
        // save the current received 'start_graph' command (to prevent it from
        // being destroyed) in order to return a correct cmd_result according to
        // it.
        TEN_ASSERT(!self->original_start_graph_cmd_of_enabling_engine,
                   "Should not happen.");
        self->original_start_graph_cmd_of_enabling_engine =
            ten_shared_ptr_clone(cmd);

        if (ten_list_size(&new_works) > 1) {
          // Create path group for these newly submitted 'start_graph' commands.
          ten_paths_create_group(
              &new_works, TEN_RESULT_RETURN_POLICY_FIRST_ERROR_OR_LAST_OK);
        }
        ten_list_clear(&new_works);
      } else {
        TEN_LOGV(
            "[%s] No more new connections should be made, enable the extension "
            "system now.",
            ten_app_get_uri(self->app));

        if (self->original_start_graph_cmd_of_enabling_engine) {
          // A previous `start_graph` command has already triggered the action
          // to establish the extension system, so we can simply respond with
          // an `OK` response to the current `start_graph` command.
          ten_engine_return_ok_for_cmd_start_graph(self, cmd);
        } else {
          // Starting the extension system is an asynchronous action, so the
          // `start_graph` command that triggers this action needs to be saved
          // first. This ensures that once the startup is successful, a response
          // can be sent to the correct `start_graph` command.
          TEN_ASSERT(!self->original_start_graph_cmd_of_enabling_engine,
                     "Should not happen.");
          self->original_start_graph_cmd_of_enabling_engine =
              ten_shared_ptr_clone(cmd);

          ten_engine_enable_extension_system(self, err);
        }
      }
    }

    ten_list_clear(&immediate_connectable_apps);
  }
}

void ten_engine_return_ok_for_cmd_start_graph(
    ten_engine_t *self, ten_shared_ptr_t *cmd_start_graph) {
  TEN_ASSERT(self, "Invalid argument");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Invalid argument");

  TEN_ASSERT(cmd_start_graph, "Invalid argument.");
  TEN_ASSERT(ten_cmd_base_check_integrity(cmd_start_graph),
             "Invalid argument.");
  TEN_ASSERT(
      ten_msg_get_type(cmd_start_graph) == TEN_MSG_TYPE_CMD_START_GRAPH,
      "The command this function handles should be a 'start_graph' command.");

  ten_engine_create_cmd_result_and_dispatch(self, cmd_start_graph,
                                            TEN_STATUS_CODE_OK, "");
}

void ten_engine_return_error_for_cmd_start_graph(
    ten_engine_t *self, ten_shared_ptr_t *cmd_start_graph, const char *fmt,
    ...) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd_start_graph, "Invalid argument.");
  TEN_ASSERT(ten_cmd_base_check_integrity(cmd_start_graph),
             "The engine should be started because of receiving a "
             "'start_graph' command.");
  TEN_ASSERT(
      ten_msg_get_type(cmd_start_graph) == TEN_MSG_TYPE_CMD_START_GRAPH,
      "The command this function handles should be a 'start_graph' command.");

  {
    // Return an error result.
    va_list ap;
    va_start(ap, fmt);

    ten_string_t *detail = ten_string_create_from_va_list(fmt, ap);

    va_end(ap);

    ten_engine_create_cmd_result_and_dispatch(self, cmd_start_graph,
                                              TEN_STATUS_CODE_ERROR,
                                              ten_string_get_raw_str(detail));
    ten_string_destroy(detail);
  }

  // The graph construction is failed, so the engine have to be closed now.
  // (There could be some 'retrying' mechanism in the protocol layer to mitigate
  // some seldom network problem, and if all the retrying are failed, this
  // function would indeed be called.)
  //
  // The closing of the engine might make the above error result unable to
  // been sent it out (because the 'if (xxx_is_closing())' checks in each
  // layer). However, some new mechanism could be invented in the future to
  // ensure the error result could be sent out successfully. So for integrity,
  // an error result is still constructed and issued above.
  //
  // TODO(Wei): There should be a such mechanism to ensure the error result to
  // be sent out successfully.
  //
  // TODO(Wei): Need to have a mechanism to prevent the engine from being
  // constructed repeatedly in a scenario contains multiple TEN app.

  // Close the engine.
  ten_engine_close_async(self);
}
