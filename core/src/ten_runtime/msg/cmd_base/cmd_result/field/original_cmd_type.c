//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/field/original_cmd_type.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_cmd_result_copy_original_cmd_type(
    ten_msg_t *self, ten_msg_t *src,
    TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && src &&
                 ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)src) &&
                 ten_raw_msg_get_type(src) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  ten_value_set_int32(
      &((ten_cmd_result_t *)self)->original_cmd_type,
      ten_raw_cmd_result_get_original_cmd_type((ten_cmd_result_t *)src));
}

bool ten_cmd_result_process_original_cmd_type(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t original_cmd_type_field;
  ten_msg_field_process_data_init(
      &original_cmd_type_field, TEN_STR_ORIGINAL_CMD_TYPE,
      &((ten_cmd_result_t *)self)->original_cmd_type, false);

  return cb(self, &original_cmd_type_field, user_data, err);
}
