//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_app_t ten_app_t;
typedef struct ten_protocol_t ten_protocol_t;

TEN_RUNTIME_PRIVATE_API bool ten_app_endpoint_listen(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_is_endpoint_closed(ten_app_t *self);
