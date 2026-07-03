//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_addon_t ten_addon_t;
typedef struct ten_addon_host_t ten_addon_host_t;

TEN_RUNTIME_API ten_addon_host_t *ten_addon_register_extension(
    const char *name, const char *base_dir, ten_addon_t *addon,
    void *register_ctx);
