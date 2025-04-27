//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"

#include <string.h>
#include <sys/types.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/field_info.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uuid.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

bool ten_raw_cmd_base_check_integrity(ten_cmd_base_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_CMD_BASE_SIGNATURE) {
    return false;
  }

  if (!ten_raw_msg_is_cmd_and_result(&self->msg_hdr)) {
    return false;
  }

  return true;
}

ten_cmd_base_t *ten_cmd_base_get_raw_cmd_base(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return (ten_cmd_base_t *)ten_shared_ptr_get_data(self);
}

bool ten_cmd_base_check_integrity(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  return ten_raw_cmd_base_check_integrity(ten_cmd_base_get_raw_cmd_base(self));
}

static void ten_raw_cmd_base_init_empty(ten_cmd_base_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_msg_init(&self->msg_hdr, TEN_MSG_TYPE_INVALID);

  ten_signature_set(&self->signature, (ten_signature_t)TEN_CMD_BASE_SIGNATURE);

  TEN_STRING_INIT(self->parent_cmd_id);
  ten_value_init_string(&self->cmd_id);
  ten_value_init_string(&self->seq_id);

  self->result_handler = NULL;
  self->result_handler_data = NULL;
}

void ten_raw_cmd_base_init(ten_cmd_base_t *self, TEN_MSG_TYPE type) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_cmd_base_init_empty(self);

  self->msg_hdr.type = type;

  switch (type) {
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_START_GRAPH);
    break;

  case TEN_MSG_TYPE_CMD_TIMEOUT:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_TIMEOUT);
    break;

  case TEN_MSG_TYPE_CMD_TIMER:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_TIMER);
    break;

  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_STOP_GRAPH);
    break;

  case TEN_MSG_TYPE_CMD_CLOSE_APP:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_CLOSE_APP);
    break;

  case TEN_MSG_TYPE_CMD_RESULT:
    ten_string_set_formatted(ten_value_peek_string(&self->msg_hdr.name), "%s",
                             TEN_STR_MSG_NAME_TEN_RESULT);
    break;

  default:
    break;
  }
}

void ten_raw_cmd_base_deinit(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_raw_msg_deinit(&self->msg_hdr);

  ten_value_deinit(&self->cmd_id);
  ten_value_deinit(&self->seq_id);

  ten_string_deinit(&self->parent_cmd_id);
}

void ten_raw_cmd_base_copy_field(ten_msg_t *self, ten_msg_t *src,
                                 ten_list_t *excluded_field_ids) {
  TEN_ASSERT(
      src && ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)src) && self,
      "Should not happen.");

  for (size_t i = 0; i < ten_cmd_base_fields_info_size; ++i) {
    if (excluded_field_ids) {
      bool skip = false;

      ten_list_foreach (excluded_field_ids, iter) {
        if (ten_cmd_base_fields_info[i].field_id ==
            ten_int32_listnode_get(iter.node)) {
          skip = true;
          break;
        }
      }

      if (skip) {
        continue;
      }
    }

    ten_msg_copy_field_func_t copy_field =
        ten_cmd_base_fields_info[i].copy_field;
    if (copy_field) {
      copy_field(self, src, excluded_field_ids);
    }
  }
}

bool ten_raw_cmd_base_process_field(ten_msg_t *self,
                                    ten_raw_msg_process_one_field_func_t cb,
                                    void *user_data, ten_error_t *err) {
  TEN_ASSERT(
      self && ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)self) && cb,
      "Should not happen.");

  for (size_t i = 0; i < ten_cmd_base_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_base_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}

static ten_string_t *ten_raw_cmd_base_gen_cmd_id_if_empty(
    ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  if (ten_string_is_empty(ten_value_peek_string(&self->cmd_id))) {
    ten_uuid4_gen_string(ten_value_peek_string(&self->cmd_id));
  }

  return ten_value_peek_string(&self->cmd_id);
}

ten_string_t *ten_cmd_base_gen_cmd_id_if_empty(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_is_cmd_and_result(self), "Should not happen.");
  return ten_raw_cmd_base_gen_cmd_id_if_empty(
      ten_cmd_base_get_raw_cmd_base(self));
}

const char *ten_raw_cmd_base_gen_new_cmd_id_forcibly(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  ten_string_t *cmd_id = ten_value_peek_string(&self->cmd_id);

  ten_string_clear(cmd_id);
  ten_uuid4_gen_string(cmd_id);

  return ten_string_get_raw_str(cmd_id);
}

const char *ten_cmd_base_gen_new_cmd_id_forcibly(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_is_cmd_and_result(self), "Should not happen.");
  return ten_raw_cmd_base_gen_new_cmd_id_forcibly(
      ten_cmd_base_get_raw_cmd_base(self));
}

void ten_raw_cmd_base_set_cmd_id(ten_cmd_base_t *self, const char *cmd_id) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self) && cmd_id,
             "Should not happen.");
  ten_string_set_formatted(ten_value_peek_string(&self->cmd_id), "%s", cmd_id);
}

ten_string_t *ten_raw_cmd_base_get_cmd_id(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");
  return ten_value_peek_string(&self->cmd_id);
}

static void ten_raw_cmd_base_save_cmd_id_to_parent_cmd_id(
    ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  ten_string_copy(&self->parent_cmd_id, ten_value_peek_string(&self->cmd_id));
}

void ten_cmd_base_save_cmd_id_to_parent_cmd_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_msg_is_cmd_and_result(self), "Should not happen.");
  ten_raw_cmd_base_save_cmd_id_to_parent_cmd_id(
      ten_cmd_base_get_raw_cmd_base(self));
}

void ten_raw_cmd_base_set_seq_id(ten_cmd_base_t *self, const char *seq_id) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self) && seq_id,
             "Should not happen.");
  ten_string_set_formatted(ten_value_peek_string(&self->seq_id), "%s", seq_id);
}

static bool ten_raw_cmd_base_cmd_id_is_empty(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");
  return ten_string_is_empty(ten_value_peek_string(&self->cmd_id));
}

bool ten_cmd_base_cmd_id_is_empty(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  return ten_raw_cmd_base_cmd_id_is_empty(ten_shared_ptr_get_data(self));
}

const char *ten_cmd_base_get_cmd_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  return ten_string_get_raw_str(
      ten_raw_cmd_base_get_cmd_id(ten_shared_ptr_get_data(self)));
}

static ten_string_t *ten_raw_cmd_base_get_parent_cmd_id(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");
  return &self->parent_cmd_id;
}

const char *ten_cmd_base_get_parent_cmd_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");

  ten_string_t *parent_cmd_id =
      ten_raw_cmd_base_get_parent_cmd_id(ten_shared_ptr_get_data(self));
  if (ten_string_is_empty(parent_cmd_id)) {
    return NULL;
  } else {
    return ten_string_get_raw_str(parent_cmd_id);
  }
}

void ten_cmd_base_set_cmd_id(ten_shared_ptr_t *self, const char *cmd_id) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  ten_raw_cmd_base_set_cmd_id(ten_shared_ptr_get_data(self), cmd_id);
}

static void ten_raw_cmd_base_reset_parent_cmd_id(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");
  ten_string_clear(&self->parent_cmd_id);
}

void ten_cmd_base_reset_parent_cmd_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  ten_raw_cmd_base_reset_parent_cmd_id(ten_shared_ptr_get_data(self));
}

ten_string_t *ten_raw_cmd_base_get_seq_id(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");
  return ten_value_peek_string(&self->seq_id);
}

const char *ten_cmd_base_get_seq_id(ten_shared_ptr_t *self) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  return ten_string_get_raw_str(
      ten_raw_cmd_base_get_seq_id(ten_shared_ptr_get_data(self)));
}

void ten_cmd_base_set_seq_id(ten_shared_ptr_t *self, const char *seq_id) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  ten_raw_cmd_base_set_seq_id(ten_shared_ptr_get_data(self), seq_id);
}

static void ten_raw_cmd_base_set_result_handler(
    ten_cmd_base_t *self,
    ten_env_transfer_msg_result_handler_func_t result_handler,
    void *result_handler_data) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  self->result_handler = result_handler;
  self->result_handler_data = result_handler_data;
}

void ten_cmd_base_set_result_handler(
    ten_shared_ptr_t *self,
    ten_env_transfer_msg_result_handler_func_t result_handler,
    void *result_handler_data) {
  TEN_ASSERT(self && ten_cmd_base_check_integrity(self), "Should not happen.");
  ten_raw_cmd_base_set_result_handler(ten_cmd_base_get_raw_cmd_base(self),
                                      result_handler, result_handler_data);
}

ten_env_transfer_msg_result_handler_func_t ten_raw_cmd_base_get_result_handler(
    ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  return self->result_handler;
}

void *ten_raw_cmd_base_get_result_handler_data(ten_cmd_base_t *self) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity(self),
             "Should not happen.");

  return self->result_handler_data;
}

bool ten_cmd_base_comes_from_client_outside(ten_shared_ptr_t *self) {
  TEN_ASSERT(
      self && ten_msg_check_integrity(self) && ten_msg_is_cmd_and_result(self),
      "Invalid argument.");

  const char *src_uri = ten_msg_get_src_app_uri(self);
  const char *cmd_id = ten_cmd_base_get_cmd_id(self);

  // The 'command ID' plays a critical role in the TEN world, so when TEN world
  // receives a command, no matter from where it receives, TEN runtime will
  // check if it contains a command ID, and assign a new command ID to it if
  // there is none in it.
  //
  // And that will give us a simple rule to determine if a command is coming
  // from the outside of the TEN world if the following assumption is true.
  //
  //    "When clients send a command to the TEN world, it can _not_
  //     specify the command ID of that command."
  //
  // Note: This is one of the few important assumptions and restrictions of the
  // TEN world.
  //
  // If the command is coming from outside of the TEN world, when that command
  // is coming to the TEN runtime, TEN runtime will assign a new command ID to
  // it, and set the source URI of that command to this command ID, in other
  // words, TEN runtime will use that command ID as the identity of the client.
  //
  // Therefore, it is a reliable way to determine if the command is coming
  // from the outside of the TEN world through checking if the src_uri and the
  // command ID of the command are equal.
  return ten_c_string_is_equal(src_uri, cmd_id);
}
