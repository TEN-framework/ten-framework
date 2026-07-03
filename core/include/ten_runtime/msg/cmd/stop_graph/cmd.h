//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_msg_t ten_msg_t;

TEN_RUNTIME_API ten_shared_ptr_t *ten_cmd_stop_graph_create(void);

TEN_RUNTIME_API const char *ten_cmd_stop_graph_get_graph_id(
    ten_shared_ptr_t *self);

TEN_RUNTIME_API bool ten_cmd_stop_graph_set_graph_id(ten_shared_ptr_t *self,
                                                     const char *graph_id);
