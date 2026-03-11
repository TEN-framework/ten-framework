//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/start_graph/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_utils/macro/check.h"

void ten_cmd_start_graph_copy_sync_stop_before_deinit(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && src && ten_raw_cmd_check_integrity((ten_cmd_t *)src) &&
                 ten_raw_msg_get_type(src) == TEN_MSG_TYPE_CMD_START_GRAPH,
             "Should not happen.");

  ten_value_set_bool(
      &((ten_cmd_start_graph_t *)self)->sync_stop_before_deinit,
      ten_raw_cmd_start_graph_get_sync_stop_before_deinit(
          (ten_cmd_start_graph_t *)src));
}

bool ten_cmd_start_graph_process_sync_stop_before_deinit(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t sync_stop_before_deinit_field;
  ten_msg_field_process_data_init(
      &sync_stop_before_deinit_field, TEN_STR_SYNC_STOP_BEFORE_DEINIT,
      &((ten_cmd_start_graph_t *)self)->sync_stop_before_deinit, false);

  return cb(self, &sync_stop_before_deinit_field, user_data, err);
}
