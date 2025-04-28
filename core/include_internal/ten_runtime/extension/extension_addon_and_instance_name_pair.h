//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/json.h"
#include "ten_utils/lib/string.h"

typedef struct ten_extension_addon_and_instance_name_pair_t {
  ten_string_t addon_name;
  ten_string_t instance_name;
} ten_extension_addon_and_instance_name_pair_t;

TEN_RUNTIME_PRIVATE_API ten_extension_addon_and_instance_name_pair_t *
ten_extension_addon_and_instance_name_pair_create(
    const char *extension_addon_name, const char *extension_instance_name);

TEN_RUNTIME_PRIVATE_API void ten_extension_addon_and_instance_name_pair_destroy(
    ten_extension_addon_and_instance_name_pair_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_addon_and_instance_name_pair_to_json(
    ten_json_t *json, const char *key, ten_string_t *addon_name,
    ten_string_t *instance_name);
