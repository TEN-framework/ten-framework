//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/close.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/msg_handling.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/path/path.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "include_internal/ten_runtime/timer/timer.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/log/log.h"

static bool ten_extension_could_be_closed(ten_extension_t *self) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  // Check if all the path timers are closed.
  return ten_list_is_empty(&self->path_timers);
}

void ten_extension_flush_remaining_paths(ten_extension_t *extension) {
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_path_table_t *path_table = extension->path_table;
  TEN_ASSERT(path_table, "Should not happen.");

  ten_list_t *in_paths = &path_table->in_paths;
  TEN_ASSERT(in_paths && ten_list_check_integrity(in_paths),
             "Should not happen.");

  // Clear the _IN_ paths of the extension.
  ten_list_clear(in_paths);

  ten_list_t *out_paths = &path_table->out_paths;
  TEN_ASSERT(out_paths && ten_list_check_integrity(out_paths),
             "Should not happen.");

  size_t out_paths_cnt = ten_list_size(out_paths);
  if (out_paths_cnt) {
    // Call ten_extension_handle_in_msg to consume cmd results, so that the
    // _OUT_paths can be removed.
    TEN_LOGD("[%s] Flushing %zu remaining out paths.",
             ten_extension_get_name(extension, true), out_paths_cnt);

    ten_list_t cmd_result_list = TEN_LIST_INIT_VAL;

    // Generate an error result for each remaining out path.
    ten_list_foreach (out_paths, iter) {
      ten_path_t *path = (ten_path_t *)ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(path && ten_path_check_integrity(path, true),
                 "Should not happen.");

      ten_shared_ptr_t *cmd_result =
          ten_cmd_result_create(TEN_STATUS_CODE_ERROR);
      TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
                 "Should not happen.");

      ten_msg_set_property(
          cmd_result, TEN_STR_DETAIL,
          ten_value_create_string(ten_string_get_raw_str(&path->cmd_id)), NULL);
      ten_cmd_base_set_cmd_id(cmd_result,
                              ten_string_get_raw_str(&path->cmd_id));
      ten_list_push_smart_ptr_back(&cmd_result_list, cmd_result);
      ten_shared_ptr_destroy(cmd_result);
    }

    // Send these newly generated error results to the extension.
    ten_list_foreach (&cmd_result_list, iter) {
      ten_shared_ptr_t *cmd_result = ten_smart_ptr_listnode_get(iter.node);
      TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
                 "Should not happen.");

      ten_extension_handle_in_msg(extension, cmd_result);
    }

    ten_list_clear(&cmd_result_list);
  }
}

// After all the path timers are closed, the closing flow could be proceed.
static void ten_extension_do_close(ten_extension_t *self) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread &&
                 ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_on_deinit(self);
}

void ten_extension_on_timer_closed(ten_timer_t *timer, void *on_closed_data) {
  TEN_ASSERT(timer, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(timer, true), "Should not happen.");

  ten_extension_t *extension = (ten_extension_t *)on_closed_data;
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_list_remove_ptr(&extension->path_timers, timer);

  if (ten_extension_could_be_closed(extension)) {
    ten_extension_do_close(extension);
  }
}

void ten_extension_do_pre_close_action(ten_extension_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(self->extension_thread, "Should not happen.");

  // Close the timers of the path tables.
  ten_list_foreach (&self->path_timers, iter) {
    ten_timer_t *timer = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(timer, "Should not happen.");

    ten_timer_stop_async(timer);
    ten_timer_close_async(timer);
  }

  if (ten_extension_could_be_closed(self)) {
    ten_extension_do_close(self);
  }
}
