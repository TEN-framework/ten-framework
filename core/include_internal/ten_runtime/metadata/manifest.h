//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "ten_utils/lib/string.h"

TEN_RUNTIME_PRIVATE_API bool ten_manifest_get_type_and_name(
    const char *filename, TEN_ADDON_TYPE *type, ten_string_t *name,
    ten_error_t *err);
