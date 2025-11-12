//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/field/status_code.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_cmd_result_copy_status_code(
    ten_msg_t *self, ten_msg_t *src,
    TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && src &&
                 ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)src) &&
                 ten_raw_msg_get_type(src) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  ten_raw_cmd_result_set_status_code(
      (ten_cmd_result_t *)self,
      ten_raw_cmd_result_get_status_code((ten_cmd_result_t *)src));
}

bool ten_cmd_result_process_status_code(ten_msg_t *self,
                                        ten_raw_msg_process_one_field_func_t cb,
                                        void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t status_code_field;
  ten_msg_field_process_data_init(&status_code_field, TEN_STR_STATUS_CODE,
                                  &((ten_cmd_result_t *)self)->status_code,
                                  false);

  return cb(self, &status_code_field, user_data, err);
}
