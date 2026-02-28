//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"

TEN_RUNTIME_API bool ten_loc_str_check_correct(const char *app_uri,
                                               const char *graph_id,
                                               const char *extension_name,
                                               ten_error_t *err);
