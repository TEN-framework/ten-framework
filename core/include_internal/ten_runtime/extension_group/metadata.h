//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_extension_group_t ten_extension_group_t;

TEN_RUNTIME_PRIVATE_API void ten_extension_group_load_metadata(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_merge_properties_from_graph(
    ten_extension_group_t *self);
