//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timeout/cmd.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timeout/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

static ten_cmd_timeout_t *get_raw_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  return (ten_cmd_timeout_t *)ten_shared_ptr_get_data(self);
}

void ten_raw_cmd_timeout_set_timer_id(ten_cmd_timeout_t *self,
                                      uint32_t timer_id) {
  TEN_ASSERT(
      self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) &&
          ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMEOUT,
      "Should not happen.");

  ten_value_set_uint32(&self->timer_id, timer_id);
}

static void ten_raw_cmd_timeout_destroy(ten_cmd_timeout_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_deinit(&self->cmd_hdr);

  ten_value_deinit(&self->timer_id);
  TEN_FREE(self);
}

void ten_raw_cmd_timeout_as_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_timeout_destroy((ten_cmd_timeout_t *)self);
}

static ten_cmd_timeout_t *ten_raw_cmd_timeout_create(const uint32_t timer_id) {
  ten_cmd_timeout_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_timeout_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(&raw_cmd->cmd_hdr, TEN_MSG_TYPE_CMD_TIMEOUT);
  ten_value_init_uint32(&raw_cmd->timer_id, timer_id);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_timeout_create(const uint32_t timer_id) {
  return ten_shared_ptr_create(ten_raw_cmd_timeout_create(timer_id),
                               ten_raw_cmd_timeout_destroy);
}

uint32_t ten_raw_cmd_timeout_get_timer_id(ten_cmd_timeout_t *self) {
  TEN_ASSERT(
      self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) &&
          ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMEOUT,
      "Should not happen.");

  return ten_value_get_uint32(&self->timer_id, NULL);
}

uint32_t ten_cmd_timeout_get_timer_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self) &&
                 ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_TIMEOUT,
             "Should not happen.");

  return ten_raw_cmd_timeout_get_timer_id(get_raw_cmd(self));
}

void ten_cmd_timeout_set_timer_id(ten_shared_ptr_t *self, uint32_t timer_id) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_timeout_set_timer_id(get_raw_cmd(self), timer_id);
}

bool ten_raw_cmd_timeout_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) && cb,
             "Should not happen.");

  for (size_t i = 0; i < ten_cmd_timeout_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_timeout_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
