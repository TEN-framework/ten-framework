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
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/trigger_life_cycle/field/field.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/trigger_life_cycle/field/stage.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

static const ten_msg_field_info_t ten_cmd_trigger_life_cycle_fields_info[] = {
    [TEN_CMD_TRIGGER_LIFE_CYCLE_FIELD_CMD_HDR] =
        {
            .field_name = NULL,
            .copy_field = ten_raw_cmd_copy_field,
            .process_field = ten_raw_cmd_process_field,
        },
    [TEN_CMD_TRIGGER_LIFE_CYCLE_FIELD_STAGE] =
        {
            .field_name = TEN_STR_STAGE,
            .copy_field = ten_cmd_trigger_life_cycle_copy_stage,
            .process_field = ten_cmd_trigger_life_cycle_process_stage,
        },
    [TEN_CMD_TRIGGER_LIFE_CYCLE_FIELD_LAST] = {0},
};

static const size_t ten_cmd_trigger_life_cycle_fields_info_size =
    sizeof(ten_cmd_trigger_life_cycle_fields_info) /
    sizeof(ten_cmd_trigger_life_cycle_fields_info[0]);
