//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/stop_graph/cmd.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/stop_graph/field/field_info.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

static ten_cmd_stop_graph_t *get_raw_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  return (ten_cmd_stop_graph_t *)ten_shared_ptr_get_data(self);
}

static void ten_raw_cmd_stop_graph_destroy(ten_cmd_stop_graph_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_deinit(&self->cmd_hdr);

  ten_value_deinit(&self->graph_id);

  TEN_FREE(self);
}

void ten_raw_cmd_stop_graph_as_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  ten_raw_cmd_stop_graph_destroy((ten_cmd_stop_graph_t *)self);
}

ten_cmd_stop_graph_t *ten_raw_cmd_stop_graph_create(void) {
  ten_cmd_stop_graph_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_stop_graph_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(&raw_cmd->cmd_hdr, TEN_MSG_TYPE_CMD_STOP_GRAPH);

  ten_value_init_string(&raw_cmd->graph_id);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_stop_graph_create(void) {
  return ten_shared_ptr_create(ten_raw_cmd_stop_graph_create(),
                               ten_raw_cmd_stop_graph_destroy);
}

static const char *ten_raw_cmd_stop_graph_get_graph_id(
    ten_cmd_stop_graph_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) &&
                 ten_raw_msg_get_type((ten_msg_t *)self) ==
                     TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");

  ten_string_t *graph_id = ten_value_peek_string(&self->graph_id);
  return ten_string_get_raw_str(graph_id);
}

const char *ten_cmd_stop_graph_get_graph_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");

  return ten_raw_cmd_stop_graph_get_graph_id(get_raw_cmd(self));
}

static bool ten_raw_cmd_stop_graph_set_graph_id(ten_cmd_stop_graph_t *self,
                                                const char *graph_id) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) &&
                 ten_raw_msg_get_type((ten_msg_t *)self) ==
                     TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");

  return ten_value_set_string(&self->graph_id, graph_id);
}

bool ten_cmd_stop_graph_set_graph_id(ten_shared_ptr_t *self,
                                     const char *graph_id) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");

  return ten_raw_cmd_stop_graph_set_graph_id(get_raw_cmd(self), graph_id);
}

bool ten_raw_cmd_stop_graph_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  for (size_t i = 0; i < ten_cmd_stop_graph_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_stop_graph_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
