//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stddef.h>

#include "include_internal/ten_runtime/msg/data/data.h"
#include "include_internal/ten_runtime/msg/data/field/buf.h"
#include "include_internal/ten_runtime/msg/data/field/field.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

static const ten_msg_field_info_t ten_data_fields_info[] = {
    [TEN_DATA_FIELD_MSGHDR] =
        {
            .field_name = NULL,
            .field_id = -1,
            .copy_field = ten_raw_msg_copy_field,
            .process_field = ten_raw_msg_process_field,
        },
    [TEN_DATA_FIELD_BUF] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_DATA_FIELD_BUF - TEN_DATA_FIELD_BUF,
            .copy_field = ten_raw_data_buf_copy,
            .process_field = ten_data_process_buf,
        },
    [TEN_DATA_FIELD_LAST] = {0},
};

static const size_t ten_data_fields_info_size =
    sizeof(ten_data_fields_info) / sizeof(ten_data_fields_info[0]);
