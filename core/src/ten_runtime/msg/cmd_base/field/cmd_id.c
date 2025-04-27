//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/field/cmd_id.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value_get.h"

void ten_cmd_base_copy_cmd_id(ten_msg_t *self, ten_msg_t *src,
                              TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_msg_check_integrity(src), "Should not happen.");

  ten_string_set_formatted(
      ten_value_peek_string(&((ten_cmd_base_t *)self)->cmd_id), "%s",
      ten_value_peek_raw_str(&((ten_cmd_base_t *)src)->cmd_id, NULL));
}

bool ten_cmd_base_process_cmd_id(ten_msg_t *self,
                                 ten_raw_msg_process_one_field_func_t cb,
                                 void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_cmd_base_t *cmd = (ten_cmd_base_t *)self;

  ten_msg_field_process_data_t cmd_id_field;
  ten_msg_field_process_data_init(&cmd_id_field, TEN_STR_CMD_ID, &cmd->cmd_id,
                                  false);

  return cb(self, &cmd_id_field, user_data, err);
}
