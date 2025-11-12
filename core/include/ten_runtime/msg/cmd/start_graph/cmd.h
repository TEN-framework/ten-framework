//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_msg_t ten_msg_t;
typedef struct ten_cmd_start_graph_t ten_cmd_start_graph_t;

TEN_RUNTIME_API ten_shared_ptr_t *ten_cmd_start_graph_create(void);

TEN_RUNTIME_API bool ten_cmd_start_graph_set_predefined_graph_name(
    ten_shared_ptr_t *self, const char *predefined_graph_name,
    ten_error_t *err);

TEN_RUNTIME_API bool ten_cmd_start_graph_set_long_running_mode(
    ten_shared_ptr_t *self, bool long_running_mode, ten_error_t *err);

TEN_RUNTIME_API bool ten_cmd_start_graph_set_graph_from_json_str(
    ten_shared_ptr_t *self, const char *json_str, ten_error_t *err);
