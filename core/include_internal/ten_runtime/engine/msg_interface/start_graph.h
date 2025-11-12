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

typedef struct ten_engine_t ten_engine_t;

TEN_RUNTIME_PRIVATE_API void ten_engine_handle_cmd_start_graph(
    ten_engine_t *self, ten_shared_ptr_t *cmd, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_engine_return_error_for_cmd_start_graph(
    ten_engine_t *self, ten_shared_ptr_t *cmd_start_graph, const char *fmt,
    ...);

TEN_RUNTIME_PRIVATE_API void ten_engine_return_ok_for_cmd_start_graph(
    ten_engine_t *self, ten_shared_ptr_t *cmd_start_graph);
