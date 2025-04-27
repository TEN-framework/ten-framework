//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"

#define TEN_CMD_SIGNATURE 0x4DAF493D677D0269U

// Every command struct should starts with this.
typedef struct ten_cmd_t {
  ten_cmd_base_t cmd_base_hdr;

  ten_signature_t signature;
} ten_cmd_t;

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_init(ten_cmd_t *self,
                                              TEN_MSG_TYPE type);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_deinit(ten_cmd_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_copy_field(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_process_field(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_cmd_check_integrity(ten_cmd_t *self);

TEN_RUNTIME_API bool ten_cmd_check_integrity(ten_shared_ptr_t *self);

TEN_RUNTIME_PRIVATE_API void ten_raw_cmd_destroy(ten_cmd_t *self);
