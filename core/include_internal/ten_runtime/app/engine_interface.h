//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_engine_t ten_engine_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_predefined_graph_info_t ten_predefined_graph_info_t;

TEN_RUNTIME_PRIVATE_API ten_engine_t *ten_app_create_engine(
    ten_app_t *self, ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API void ten_app_del_engine(ten_app_t *self,
                                                ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API ten_predefined_graph_info_t *
ten_app_get_singleton_predefined_graph_info_by_name(ten_app_t *self,
                                                    const char *graph_name);

TEN_RUNTIME_PRIVATE_API ten_engine_t *ten_app_get_engine_by_graph_id(
    ten_app_t *self, const char *graph_id);
