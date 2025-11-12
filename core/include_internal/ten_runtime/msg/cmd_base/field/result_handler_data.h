//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"

typedef struct ten_msg_t ten_msg_t;

TEN_RUNTIME_PRIVATE_API void ten_cmd_base_copy_result_handler_data(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids);
