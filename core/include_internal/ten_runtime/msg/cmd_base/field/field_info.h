//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/cmd_id.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/field.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/result_handler.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/result_handler_data.h"
#include "include_internal/ten_runtime/msg/cmd_base/field/seq_id.h"
#include "include_internal/ten_runtime/msg/field/field.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

static const ten_msg_field_info_t ten_cmd_base_fields_info[] = {
    [TEN_CMD_BASE_FIELD_MSGHDR] =
        {
            .field_name = NULL,
            .field_id = -1,
            .copy_field = ten_raw_msg_copy_field,
            .process_field = ten_raw_msg_process_field,
        },
    [TEN_CMD_BASE_FIELD_CMD_ID] =
        {
            .field_name = TEN_STR_CMD_ID,
            .field_id = TEN_MSG_FIELD_LAST + TEN_CMD_BASE_FIELD_CMD_ID,
            .copy_field = ten_cmd_base_copy_cmd_id,
            .process_field = ten_cmd_base_process_cmd_id,
        },
    [TEN_CMD_BASE_FIELD_SEQ_ID] =
        {
            .field_name = TEN_STR_SEQ_ID,
            .field_id = TEN_MSG_FIELD_LAST + TEN_CMD_BASE_FIELD_SEQ_ID,
            .copy_field = ten_cmd_base_copy_seq_id,
            .process_field = ten_cmd_base_process_seq_id,
        },
    [TEN_CMD_BASE_FIELD_RESPONSE_HANDLER] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_CMD_BASE_FIELD_RESPONSE_HANDLER,
            .copy_field = ten_cmd_base_copy_result_handler,
            .process_field = NULL,
        },
    [TEN_CMD_BASE_FIELD_RESPONSE_HANDLER_DATA] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_CMD_BASE_FIELD_RESPONSE_HANDLER_DATA,
            .copy_field = ten_cmd_base_copy_result_handler_data,
            .process_field = NULL,
        },
    [TEN_CMD_BASE_FIELD_LAST] = {0},
};

static const size_t ten_cmd_base_fields_info_size =
    sizeof(ten_cmd_base_fields_info) / sizeof(ten_cmd_base_fields_info[0]);
