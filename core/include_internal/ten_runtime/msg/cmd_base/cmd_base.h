//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/ten_env/internal/send.h"
#include "ten_utils/lib/smart_ptr.h"

#define TEN_CMD_BASE_SIGNATURE 0x0DF810096247FFD5U

typedef struct ten_connection_t ten_connection_t;

// Every command struct should starts with this.
typedef struct ten_cmd_base_t {
  ten_msg_t msg_hdr;

  ten_signature_t signature;

  // If the command is cloned from another command, this field is used to create
  // the relationship between these 2 commands.
  ten_string_t parent_cmd_id;

  ten_value_t cmd_id;  // string. This is used in TEN runtime internally.
  ten_value_t seq_id;  // string. This is used in TEN client.

  ten_env_transfer_msg_result_handler_func_t result_handler;
  void *result_handler_data;
} ten_cmd_base_t;

TEN_RUNTIME_API bool ten_raw_cmd_base_check_integrity(ten_cmd_base_t *self);

TEN_RUNTIME_API bool ten_cmd_base_check_integrity(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_cmd_base_gen_new_cmd_id_forcibly(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_raw_cmd_base_gen_new_cmd_id_forcibly(
    ten_cmd_base_t *self);

TEN_RUNTIME_PRIVATE_API ten_cmd_base_t *ten_cmd_base_get_raw_cmd_base(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_cmd_base_get_seq_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_set_seq_id(ten_shared_ptr_t *self,
                                                     const char *seq_id);

TEN_RUNTIME_PRIVATE_API const char *ten_cmd_base_get_cmd_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_set_cmd_id(ten_shared_ptr_t *self,
                                                     const char *cmd_id);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_base_init(ten_cmd_base_t *self,
                                                   TEN_MSG_TYPE type);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_base_deinit(ten_cmd_base_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_base_copy_field(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_base_process_field(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_cmd_base_gen_cmd_id_if_empty(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_base_set_cmd_id(ten_cmd_base_t *self,
                                                         const char *cmd_id);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_raw_cmd_base_get_cmd_id(
    ten_cmd_base_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_save_cmd_id_to_parent_cmd_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_base_set_seq_id(ten_cmd_base_t *self,
                                                         const char *seq_id);

TEN_RUNTIME_PRIVATE_API bool ten_cmd_base_cmd_id_is_empty(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_cmd_base_get_parent_cmd_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_reset_parent_cmd_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_raw_cmd_base_get_seq_id(
    ten_cmd_base_t *self);

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_set_result_handler(
    ten_shared_ptr_t *self,
    ten_env_transfer_msg_result_handler_func_t result_handler,
    void *result_handler_data);

TEN_RUNTIME_PRIVATE_API ten_env_transfer_msg_result_handler_func_t
ten_raw_cmd_base_get_result_handler(ten_cmd_base_t *self);

TEN_RUNTIME_PRIVATE_API void *ten_raw_cmd_base_get_result_handler_data(
    ten_cmd_base_t *self);

/**
 * @brief Whether this cmd comes from the client outside of TEN world, e.g.:
 * browsers.
 */
TEN_RUNTIME_PRIVATE_API bool ten_cmd_base_comes_from_client_outside(
    ten_shared_ptr_t *self);
