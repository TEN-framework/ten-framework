//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/mutex.h"

typedef struct ten_app_t ten_app_t;

TEN_RUNTIME_PRIVATE_API ten_list_t g_apps;
TEN_RUNTIME_PRIVATE_API ten_mutex_t *g_apps_mutex;

TEN_RUNTIME_API void ten_global_init(void);

TEN_RUNTIME_API void ten_global_deinit(void);

TEN_RUNTIME_PRIVATE_API void ten_global_add_app(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API void ten_global_del_app(ten_app_t *self);
