//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"

typedef struct ten_metadata_info_t ten_metadata_info_t;

TEN_RUNTIME_PRIVATE_API void ten_set_default_manifest_info(
    const char *base_dir, ten_metadata_info_t *manifest, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_set_default_property_info(
    const char *base_dir, ten_metadata_info_t *property, ten_error_t *err);
