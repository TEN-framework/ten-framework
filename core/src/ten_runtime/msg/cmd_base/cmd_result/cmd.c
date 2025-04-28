//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"

#include <stdbool.h>
#include <stdlib.h>

#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/schema_store/cmd.h"
#include "include_internal/ten_runtime/schema_store/msg.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"

static bool ten_raw_cmd_result_check_integrity(ten_cmd_result_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_CMD_STATUS_SIGNATURE) {
    return false;
  }

  if (!ten_raw_msg_is_cmd_result(&self->cmd_base_hdr.msg_hdr)) {
    return false;
  }

  return true;
}

static bool ten_cmd_result_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_raw_cmd_result_check_integrity(ten_shared_ptr_get_data(self)) ==
      false) {
    return false;
  }
  return true;
}

static ten_cmd_result_t *ten_cmd_result_get_raw_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");

  return (ten_cmd_result_t *)ten_shared_ptr_get_data(self);
}

void ten_raw_cmd_result_destroy(ten_cmd_result_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_base_deinit(&self->cmd_base_hdr);
  ten_signature_set(&self->signature, 0);
  ten_string_deinit(ten_value_peek_string(&self->original_cmd_name));

  TEN_FREE(self);
}

static ten_cmd_result_t *ten_raw_cmd_result_create_empty(void) {
  ten_cmd_result_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_result_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_base_init(&raw_cmd->cmd_base_hdr, TEN_MSG_TYPE_CMD_RESULT);

  ten_signature_set(&raw_cmd->signature,
                    (ten_signature_t)TEN_CMD_STATUS_SIGNATURE);

  ten_value_init_int32(&raw_cmd->status_code, TEN_STATUS_CODE_INVALID);

  // We will get the original cmd type later.
  ten_value_init_int32(&raw_cmd->original_cmd_type, TEN_MSG_TYPE_INVALID);
  ten_value_init_string(&raw_cmd->original_cmd_name);

  // By default, every `cmd_result` is a final cmd_result. However, users can
  // manually set a `cmd_result` to _not_ be a final cmd_result.
  ten_value_init_bool(&raw_cmd->is_final, true);

  // Whether a `cmd_result` is completed will be determined by the path_table,
  // so by default, it is not completed at the beginning.
  ten_value_init_bool(&raw_cmd->is_completed, false);

  return raw_cmd;
}

static ten_cmd_result_t *ten_raw_cmd_result_create(
    const TEN_STATUS_CODE status_code) {
  ten_cmd_result_t *raw_cmd = ten_raw_cmd_result_create_empty();

  ten_value_set_int32(&raw_cmd->status_code, status_code);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_result_create(const TEN_STATUS_CODE status_code) {
  return ten_shared_ptr_create(ten_raw_cmd_result_create(status_code),
                               ten_raw_cmd_result_destroy);
}

static void ten_raw_cmd_result_set_original_cmd_name(
    ten_cmd_result_t *self, const char *original_cmd_name) {
  TEN_ASSERT(self && ten_raw_cmd_result_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(original_cmd_name && strlen(original_cmd_name),
             "Invalid argument.");

  ten_string_set_from_c_str(ten_value_peek_string(&self->original_cmd_name),
                            original_cmd_name);
}

static ten_cmd_result_t *ten_raw_cmd_result_create_from_raw_cmd(
    const TEN_STATUS_CODE status_code, ten_cmd_t *original_cmd) {
  ten_cmd_result_t *cmd = ten_raw_cmd_result_create(status_code);

  if (original_cmd) {
    // @{
    ten_raw_cmd_base_set_cmd_id(
        (ten_cmd_base_t *)cmd,
        ten_string_get_raw_str(
            ten_raw_cmd_base_get_cmd_id((ten_cmd_base_t *)original_cmd)));
    ten_raw_cmd_base_set_seq_id(
        (ten_cmd_base_t *)cmd,
        ten_string_get_raw_str(
            ten_raw_cmd_base_get_seq_id((ten_cmd_base_t *)original_cmd)));
    // @}

    ten_raw_cmd_result_set_original_cmd_type(
        cmd, ten_raw_msg_get_type((ten_msg_t *)original_cmd));

    ten_raw_cmd_result_set_original_cmd_name(
        cmd, ten_raw_msg_get_name((ten_msg_t *)original_cmd));

    // There are only 2 possible values of destination count of
    // 'original_cmd':
    // - 0
    //   The original_cmd is sent to a extension, and the cmd result is
    //   generated from that extension. Because TEN runtime would clear the
    //   destination locations of the original_cmd, the destination count
    //   would therefore be 0.
    // - 1
    //   In all other situations, the destination count of original_cmd should
    //   be 1, and the source location of the cmd result would be that
    //   destination of the original_cmd, therefore, we handle it here.
    TEN_ASSERT(ten_raw_msg_get_dest_cnt((ten_msg_t *)original_cmd) <= 1,
               "Should not happen.");
    if (ten_raw_msg_get_dest_cnt((ten_msg_t *)original_cmd) == 1) {
      ten_loc_t *dest_loc =
          ten_raw_msg_get_first_dest_loc((ten_msg_t *)original_cmd);
      ten_raw_msg_set_src_to_loc((ten_msg_t *)cmd, dest_loc);
    }

    ten_raw_msg_clear_and_set_dest_to_loc(
        (ten_msg_t *)cmd, ten_raw_msg_get_src_loc((ten_msg_t *)original_cmd));
  }

  return cmd;
}

static ten_cmd_result_t *ten_raw_cmd_result_create_from_cmd(
    const TEN_STATUS_CODE status_code, ten_shared_ptr_t *original_cmd) {
  return ten_raw_cmd_result_create_from_raw_cmd(
      status_code, original_cmd ? ten_shared_ptr_get_data(original_cmd) : NULL);
}

ten_shared_ptr_t *ten_cmd_result_create_from_cmd(
    const TEN_STATUS_CODE status_code, ten_shared_ptr_t *original_cmd) {
  return ten_shared_ptr_create(
      ten_raw_cmd_result_create_from_cmd(status_code, original_cmd),
      ten_raw_cmd_result_destroy);
}

TEN_STATUS_CODE ten_raw_cmd_result_get_status_code(ten_cmd_result_t *self) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_value_get_int32(&self->status_code, NULL);
}

TEN_STATUS_CODE ten_cmd_result_get_status_code(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_raw_cmd_result_get_status_code(ten_shared_ptr_get_data(self));
}

bool ten_raw_cmd_result_set_final(ten_cmd_result_t *self, bool is_final,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  ten_value_set_bool(&self->is_final, is_final);

  return true;
}

bool ten_raw_cmd_result_set_completed(ten_cmd_result_t *self, bool is_completed,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  ten_value_set_bool(&self->is_completed, is_completed);

  return true;
}

bool ten_cmd_result_set_completed(ten_shared_ptr_t *self, bool is_completed,
                                  ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_raw_cmd_result_set_completed(
      (ten_cmd_result_t *)ten_msg_get_raw_msg(self), is_completed, err);
}

bool ten_cmd_result_set_final(ten_shared_ptr_t *self, bool is_final,
                              ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_raw_cmd_result_set_final(
      (ten_cmd_result_t *)ten_msg_get_raw_msg(self), is_final, err);
}

bool ten_raw_cmd_result_is_final(ten_cmd_result_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_value_get_bool(&self->is_final, err);
}

bool ten_raw_cmd_result_is_completed(ten_cmd_result_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "self should not be NULL.");
  TEN_ASSERT(ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD_RESULT,
             "msg type should be TEN_MSG_TYPE_CMD_RESULT.");

  return ten_value_get_bool(&self->is_completed, err);
}

bool ten_cmd_result_is_final(ten_shared_ptr_t *self, ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_raw_cmd_result_is_final(
      (ten_cmd_result_t *)ten_msg_get_raw_msg(self), err);
}

bool ten_cmd_result_is_completed(ten_shared_ptr_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "self should not be NULL.");
  TEN_ASSERT(ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "msg type should be TEN_MSG_TYPE_CMD_RESULT.");

  return ten_raw_cmd_result_is_completed(
      (ten_cmd_result_t *)ten_msg_get_raw_msg(self), err);
}

void ten_raw_cmd_result_set_status_code(ten_cmd_result_t *self,
                                        TEN_STATUS_CODE status_code) {
  TEN_ASSERT(self && ten_raw_cmd_result_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(status_code != TEN_STATUS_CODE_INVALID, "Invalid argument.");

  ten_value_set_int32(&self->status_code, status_code);
}

void ten_cmd_result_set_status_code(ten_shared_ptr_t *self,
                                    TEN_STATUS_CODE status_code) {
  TEN_ASSERT(self && ten_cmd_result_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(status_code != TEN_STATUS_CODE_INVALID, "Invalid argument.");

  ten_cmd_result_t *cmd_result = ten_cmd_result_get_raw_cmd(self);
  ten_raw_cmd_result_set_status_code(cmd_result, status_code);
}

void ten_raw_cmd_result_set_original_cmd_type(ten_cmd_result_t *self,
                                              TEN_MSG_TYPE type) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");
  ten_value_set_int32(&self->original_cmd_type, type);
}

TEN_MSG_TYPE ten_raw_cmd_result_get_original_cmd_type(ten_cmd_result_t *self) {
  TEN_ASSERT(self && ten_raw_msg_get_type((ten_msg_t *)self) ==
                         TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  return ten_value_get_int32(&self->original_cmd_type, NULL);
}

void ten_cmd_result_set_original_cmd_type(ten_shared_ptr_t *self,
                                          TEN_MSG_TYPE type) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");
  ten_raw_cmd_result_set_original_cmd_type(ten_cmd_result_get_raw_cmd(self),
                                           type);
}

TEN_MSG_TYPE ten_cmd_result_get_original_cmd_type(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_get_type(self) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");
  return ten_raw_cmd_result_get_original_cmd_type(
      ten_cmd_result_get_raw_cmd(self));
}

ten_msg_t *ten_raw_cmd_result_as_msg_clone(
    ten_msg_t *self, TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)self),
             "Should not happen.");

  ten_cmd_result_t *cloned_cmd = ten_raw_cmd_result_create_empty();

  for (size_t i = 0; i < ten_cmd_result_fields_info_size; ++i) {
    ten_msg_copy_field_func_t copy_field =
        ten_cmd_result_fields_info[i].copy_field;
    if (copy_field) {
      copy_field((ten_msg_t *)cloned_cmd, self, NULL);
    }
  }

  return (ten_msg_t *)cloned_cmd;
}

bool ten_raw_cmd_result_validate_schema(ten_msg_t *status_msg,
                                        ten_schema_store_t *schema_store,
                                        bool is_msg_out, ten_error_t *err) {
  TEN_ASSERT(status_msg && ten_raw_msg_check_integrity(status_msg),
             "Invalid argument.");
  TEN_ASSERT(ten_raw_msg_get_type(status_msg) == TEN_MSG_TYPE_CMD_RESULT,
             "Invalid argument.");
  TEN_ASSERT(schema_store && ten_schema_store_check_integrity(schema_store),
             "Invalid argument.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  ten_cmd_result_t *cmd_result = (ten_cmd_result_t *)status_msg;
  TEN_ASSERT(cmd_result && ten_raw_cmd_result_check_integrity(cmd_result),
             "Invalid argument.");

  const char *original_cmd_name =
      ten_value_peek_raw_str(&cmd_result->original_cmd_name, err);
  TEN_ASSERT(original_cmd_name && strlen(original_cmd_name),
             "Invalid argument.");

  bool result = true;

  do {
    // The status `out` is responding to the cmd `in`, ex: we will call
    // `return_status` in `on_cmd`. The schema of status `out` defines within
    // the corresponding cmd `in`. So we need to reverse the `is_msg_out` here.
    ten_msg_schema_t *original_msg_schema = ten_schema_store_get_msg_schema(
        schema_store, ten_raw_msg_get_type(status_msg), original_cmd_name,
        !is_msg_out);
    if (!original_msg_schema) {
      break;
    }

    ten_cmd_schema_t *original_cmd_schema =
        (ten_cmd_schema_t *)original_msg_schema;
    TEN_ASSERT(original_cmd_schema &&
                   ten_cmd_schema_check_integrity(original_cmd_schema),
               "Invalid argument.");

    if (!ten_cmd_schema_adjust_cmd_result_properties(
            original_cmd_schema, &status_msg->properties, err)) {
      result = false;
      break;
    }

    if (!ten_cmd_schema_validate_cmd_result_properties(
            original_cmd_schema, &status_msg->properties, err)) {
      result = false;
      break;
    }
  } while (0);

  return result;
}

void ten_cmd_result_set_original_cmd_name(ten_shared_ptr_t *self,
                                          const char *original_cmd_name) {
  TEN_ASSERT(self && ten_cmd_result_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(original_cmd_name && strlen(original_cmd_name),
             "Invalid argument.");

  ten_raw_cmd_result_set_original_cmd_name(
      (ten_cmd_result_t *)ten_msg_get_raw_msg(self), original_cmd_name);
}

bool ten_raw_cmd_result_loop_all_fields(ten_msg_t *self,
                                        ten_raw_msg_process_one_field_func_t cb,
                                        void *user_data, ten_error_t *err) {
  TEN_ASSERT(
      self && ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)self) && cb,
      "Should not happen.");

  for (size_t i = 0; i < ten_cmd_result_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_result_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
