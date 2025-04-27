//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_env_proxy_t ten_env_proxy_t;

TEN_RUNTIME_PRIVATE_API void ten_env_add_ten_proxy(
    ten_env_t *self, ten_env_proxy_t *ten_env_proxy);

TEN_RUNTIME_PRIVATE_API void ten_env_delete_ten_proxy(
    ten_env_t *self, ten_env_proxy_t *ten_env_proxy);
