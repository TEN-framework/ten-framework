//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

typedef struct ten_extension_t ten_extension_t;
typedef struct ten_timer_t ten_timer_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;

TEN_RUNTIME_PRIVATE_API void ten_extension_do_pre_close_action(
    ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_timer_closed(
    ten_timer_t *timer, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_path_timer_closed(
    ten_timer_t *timer, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_extension_flush_remaining_paths(
    ten_extension_t *extension);
