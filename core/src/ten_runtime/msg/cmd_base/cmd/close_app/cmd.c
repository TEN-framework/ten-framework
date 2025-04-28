//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/msg/cmd/close_app/cmd.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/close_app/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/close_app/field/field_info.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

static void ten_raw_cmd_close_app_destroy(ten_cmd_close_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_deinit(&self->cmd_hdr);
  TEN_FREE(self);
}

void ten_raw_cmd_close_app_as_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_close_app_destroy((ten_cmd_close_app_t *)self);
}

ten_cmd_close_app_t *ten_raw_cmd_close_app_create(void) {
  ten_cmd_close_app_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_close_app_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(&raw_cmd->cmd_hdr, TEN_MSG_TYPE_CMD_CLOSE_APP);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_close_app_create(void) {
  return ten_shared_ptr_create(ten_raw_cmd_close_app_create(),
                               ten_raw_cmd_close_app_destroy);
}

bool ten_raw_cmd_close_app_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) && cb,
             "Should not happen.");

  for (size_t i = 0; i < ten_cmd_close_app_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_close_app_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
