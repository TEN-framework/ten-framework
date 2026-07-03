//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/ten_env/ten_env.h"

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_on_init_done(ten_env_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_addon_loader_on_deinit_done(ten_env_t *self);
