//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/macro/check.h"

bool ten_cmd_timer_process_timeout_us(ten_msg_t *self,
                                      ten_raw_msg_process_one_field_func_t cb,
                                      void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t timeout_us_field;
  ten_msg_field_process_data_init(&timeout_us_field, TEN_STR_TIMEOUT_US,
                                  &((ten_cmd_timer_t *)self)->timeout_us,
                                  false);

  return cb(self, &timeout_us_field, user_data, err);
}
