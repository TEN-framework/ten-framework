//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/cmd.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"

static ten_cmd_timer_t *get_raw_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");

  return (ten_cmd_timer_t *)ten_shared_ptr_get_data(self);
}

bool ten_raw_cmd_timer_set_timer_id(ten_cmd_timer_t *self, uint32_t timer_id) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_set_uint32(&self->timer_id, timer_id);
}

bool ten_raw_cmd_timer_set_times(ten_cmd_timer_t *self, int32_t times) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_set_int32(&self->times, times);
}

static void ten_raw_cmd_timer_destroy(ten_cmd_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_deinit(&self->cmd_hdr);

  ten_value_deinit(&self->timer_id);
  ten_value_deinit(&self->timeout_us);
  ten_value_deinit(&self->times);

  TEN_FREE(self);
}

void ten_raw_cmd_timer_as_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_timer_destroy((ten_cmd_timer_t *)self);
}

bool ten_raw_cmd_timer_loop_all_fields(ten_msg_t *self,
                                       ten_raw_msg_process_one_field_func_t cb,
                                       void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  for (size_t i = 0; i < ten_cmd_timer_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_timer_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}

ten_cmd_timer_t *ten_raw_cmd_timer_create(void) {
  ten_cmd_timer_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_timer_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(&raw_cmd->cmd_hdr, TEN_MSG_TYPE_CMD_TIMER);

  ten_value_init_uint32(&raw_cmd->timer_id, 0);
  ten_value_init_uint64(&raw_cmd->timeout_us, 0);
  ten_value_init_int32(&raw_cmd->times, 0);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_timer_create(void) {
  return ten_shared_ptr_create(ten_raw_cmd_timer_create(),
                               ten_raw_cmd_timer_destroy);
}

uint32_t ten_raw_cmd_timer_get_timer_id(ten_cmd_timer_t *self) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_get_uint32(&self->timer_id, NULL);
}

uint32_t ten_cmd_timer_get_timer_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_TIMER,
             "Should not happen.");

  return ten_raw_cmd_timer_get_timer_id(get_raw_cmd(self));
}

uint64_t ten_raw_cmd_timer_get_timeout_us(ten_cmd_timer_t *self) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_get_uint64(&self->timeout_us, NULL);
}

uint64_t ten_cmd_timer_get_timeout_us(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_TIMER,
             "Should not happen.");

  return ten_raw_cmd_timer_get_timeout_us(get_raw_cmd(self));
}

static bool ten_raw_cmd_timer_set_timeout_us(ten_cmd_timer_t *self,
                                             uint64_t timeout_us) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_set_uint64(&self->timeout_us, timeout_us);
}

bool ten_cmd_timer_set_timeout_us(ten_shared_ptr_t *self, uint64_t timeout_us) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_TIMER,
             "Should not happen.");

  return ten_raw_cmd_timer_set_timeout_us(get_raw_cmd(self), timeout_us);
}

int32_t ten_raw_cmd_timer_get_times(ten_cmd_timer_t *self) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_TIMER,
      "Should not happen.");

  return ten_value_get_int32(&self->times, NULL);
}

bool ten_raw_cmd_timer_set_ten_property(ten_msg_t *self, ten_list_t *paths,
                                        ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(paths && ten_list_check_integrity(paths),
             "path should not be empty.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  bool success = true;

  ten_error_t tmp_err;
  bool use_tmp_err = false;
  if (!err) {
    use_tmp_err = true;
    ten_error_init(&tmp_err);
    err = &tmp_err;
  }

  ten_cmd_timer_t *timer_cmd = (ten_cmd_timer_t *)self;

  ten_list_foreach (paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if (!strcmp(TEN_STR_TIMER_ID,
                  ten_string_get_raw_str(&item->obj_item_str))) {
        ten_value_set_uint32(&timer_cmd->timer_id,
                             ten_value_get_uint32(value, err));
        success = ten_error_is_success(err);
      } else if (!strcmp(TEN_STR_TIMEOUT_US,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        ten_value_set_uint64(&timer_cmd->timeout_us,
                             ten_value_get_uint64(value, err));
        success = ten_error_is_success(err);
      } else if (!strcmp(TEN_STR_TIMES,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        ten_value_set_int32(&timer_cmd->times, ten_value_get_int32(value, err));
        success = ten_error_is_success(err);
      } else if (!strcmp(TEN_STR_NAME,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        if (ten_value_is_string(value)) {
          ten_value_set_string_with_size(
              &self->name, ten_value_peek_raw_str(value, &tmp_err),
              strlen(ten_value_peek_raw_str(value, &tmp_err)));
          success = true;
        } else {
          success = false;
        }
      }
      break;
    }

    default:
      break;
    }
  }

  if (use_tmp_err) {
    ten_error_deinit(&tmp_err);
  }

  return success;
}

int32_t ten_cmd_timer_get_times(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_TIMER,
             "Should not happen.");

  return ten_raw_cmd_timer_get_times(get_raw_cmd(self));
}

bool ten_cmd_timer_set_timer_id(ten_shared_ptr_t *self, uint32_t timer_id) {
  return ten_raw_cmd_timer_set_timer_id(get_raw_cmd(self), timer_id);
}

bool ten_cmd_timer_set_times(ten_shared_ptr_t *self, int32_t times) {
  return ten_raw_cmd_timer_set_times(get_raw_cmd(self), times);
}
