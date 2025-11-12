//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"

typedef struct ten_extension_info_t ten_extension_info_t;

TEN_RUNTIME_PRIVATE_API bool ten_extension_info_to_json(
    ten_extension_info_t *self, ten_json_t *info);

TEN_RUNTIME_PRIVATE_API int ten_extension_info_connections_to_json(
    ten_extension_info_t *self, ten_json_t *json, ten_error_t *err);
