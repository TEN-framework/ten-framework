//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timeout/field/timer_id.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timeout/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/cmd.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/macro/check.h"

bool ten_cmd_timeout_process_timer_id(ten_msg_t *self,
                                      ten_raw_msg_process_one_field_func_t cb,
                                      void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t timer_id_field;
  ten_msg_field_process_data_init(&timer_id_field, TEN_STR_TIMER_ID,
                                  &((ten_cmd_timeout_t *)self)->timer_id,
                                  false);

  return cb(self, &timer_id_field, user_data, err);
}
