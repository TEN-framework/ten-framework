//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/trigger_life_cycle/field/stage.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/trigger_life_cycle/cmd.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_cmd_trigger_life_cycle_copy_stage(
    ten_msg_t *self, ten_msg_t *src,
    TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(
      self && src && ten_raw_cmd_check_integrity((ten_cmd_t *)src) &&
          ten_raw_msg_get_type(src) == TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE,
      "Should not happen.");

  ten_string_copy(
      ten_value_peek_string(&((ten_cmd_trigger_life_cycle_t *)self)->stage),
      ten_value_peek_string(&((ten_cmd_trigger_life_cycle_t *)src)->stage));
}

bool ten_cmd_trigger_life_cycle_process_stage(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t stage_field;
  ten_msg_field_process_data_init(
      &stage_field, TEN_STR_STAGE,
      &((ten_cmd_trigger_life_cycle_t *)self)->stage, false);

  return cb(self, &stage_field, user_data, err);
}
