//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

typedef struct ten_app_t ten_app_t;

TEN_RUNTIME_PRIVATE_API void ten_app_service_hub_create_metric(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API void ten_app_service_hub_destroy_metric(
    ten_app_t *self);
