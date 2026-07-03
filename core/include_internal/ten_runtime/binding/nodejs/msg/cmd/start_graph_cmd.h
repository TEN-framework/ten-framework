//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_cmd_start_graph_module_init(napi_env env, napi_value exports);
