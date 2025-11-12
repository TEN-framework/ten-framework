//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"

typedef struct ten_engine_t ten_engine_t;

TEN_RUNTIME_PRIVATE_API bool ten_engine_enable_extension_system(
    ten_engine_t *self, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void
ten_engine_find_extension_info_for_all_extensions_of_extension_thread_task(
    void *self_, void *arg);
