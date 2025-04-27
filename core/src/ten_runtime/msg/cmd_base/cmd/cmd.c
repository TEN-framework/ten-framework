//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"

#include <sys/types.h>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/custom/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/field/field_info.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/stop_graph/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg_info.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

bool ten_raw_cmd_check_integrity(ten_cmd_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_CMD_SIGNATURE) {
    return false;
  }

  if (!ten_raw_msg_is_cmd(&self->cmd_base_hdr.msg_hdr)) {
    return false;
  }

  return true;
}

static ten_cmd_t *ten_cmd_get_raw_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return (ten_cmd_t *)ten_shared_ptr_get_data(self);
}

bool ten_cmd_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_raw_cmd_check_integrity(ten_cmd_get_raw_cmd(self)) == false) {
    return false;
  }
  return true;
}

void ten_raw_cmd_init(ten_cmd_t *self, TEN_MSG_TYPE type) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_base_init(&self->cmd_base_hdr, type);
  ten_signature_set(&self->signature, (ten_signature_t)TEN_CMD_SIGNATURE);
}

void ten_raw_cmd_deinit(ten_cmd_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity(self), "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_raw_cmd_base_deinit(&self->cmd_base_hdr);
}

void ten_raw_cmd_copy_field(ten_msg_t *self, ten_msg_t *src,
                            ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_cmd_check_integrity((ten_cmd_t *)src) && self,
             "Should not happen.");

  for (size_t i = 0; i < ten_cmd_fields_info_size; ++i) {
    if (excluded_field_ids) {
      bool skip = false;

      ten_list_foreach (excluded_field_ids, iter) {
        if (ten_cmd_fields_info[i].field_id ==
            ten_int32_listnode_get(iter.node)) {
          skip = true;
          break;
        }
      }

      if (skip) {
        continue;
      }
    }

    ten_msg_copy_field_func_t copy_field = ten_cmd_fields_info[i].copy_field;
    if (copy_field) {
      copy_field(self, src, excluded_field_ids);
    }
  }
}

bool ten_raw_cmd_process_field(ten_msg_t *self,
                               ten_raw_msg_process_one_field_func_t cb,
                               void *user_data, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) && cb,
             "Should not happen.");

  for (size_t i = 0; i < ten_cmd_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}

void ten_raw_cmd_destroy(ten_cmd_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  switch (self->cmd_base_hdr.msg_hdr.type) {
  case TEN_MSG_TYPE_CMD:
    ten_raw_cmd_custom_as_msg_destroy((ten_msg_t *)self);
    break;
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
    ten_raw_cmd_stop_graph_as_msg_destroy((ten_msg_t *)self);
    break;
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
    ten_raw_cmd_close_app_as_msg_destroy((ten_msg_t *)self);
    break;
  case TEN_MSG_TYPE_CMD_TIMEOUT:
    ten_raw_cmd_timeout_as_msg_destroy((ten_msg_t *)self);
    break;
  case TEN_MSG_TYPE_CMD_TIMER:
    ten_raw_cmd_timer_as_msg_destroy((ten_msg_t *)self);
    break;
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    ten_raw_cmd_start_graph_as_msg_destroy((ten_msg_t *)self);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

static TEN_MSG_TYPE ten_cmd_type_from_name_string(const char *name_str) {
  TEN_MSG_TYPE msg_type = TEN_MSG_TYPE_CMD;

  for (size_t i = 0; i < ten_msg_info_size; i++) {
    if (ten_msg_info[i].msg_unique_name &&
        ten_c_string_is_equal(name_str, ten_msg_info[i].msg_unique_name)) {
      msg_type = (TEN_MSG_TYPE)i;
      break;
    }
  }

  switch (msg_type) {
  case TEN_MSG_TYPE_INVALID:
  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
  case TEN_MSG_TYPE_CMD_RESULT:
    return TEN_MSG_TYPE_INVALID;

  default:
    return msg_type;
  }
}

static ten_cmd_t *ten_raw_cmd_create(const char *name, ten_error_t *err) {
  if (!name) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to create cmd without a name.");
    }
    return NULL;
  }

  TEN_MSG_TYPE cmd_type = ten_cmd_type_from_name_string(name);

  switch (cmd_type) {
  case TEN_MSG_TYPE_CMD:
    return ten_raw_cmd_custom_create(name, err);
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
    return (ten_cmd_t *)ten_raw_cmd_stop_graph_create();
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
    return (ten_cmd_t *)ten_raw_cmd_close_app_create();
  case TEN_MSG_TYPE_CMD_TIMER:
    return (ten_cmd_t *)ten_raw_cmd_timer_create();
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    return (ten_cmd_t *)ten_raw_cmd_start_graph_create();

  default:
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

ten_shared_ptr_t *ten_cmd_create(const char *name, ten_error_t *err) {
  ten_cmd_t *cmd = ten_raw_cmd_create(name, err);
  if (!cmd) {
    return NULL;
  }
  return ten_shared_ptr_create(cmd, ten_raw_cmd_destroy);
}
