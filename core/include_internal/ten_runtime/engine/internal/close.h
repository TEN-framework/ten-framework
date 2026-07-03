//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/timer/timer.h"

typedef struct ten_engine_t ten_engine_t;
typedef struct ten_extension_context_t ten_extension_context_t;

typedef void (*ten_engine_on_closed_func_t)(ten_engine_t *self,
                                            void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_engine_close_async(ten_engine_t *self);

TEN_RUNTIME_PRIVATE_API void ten_engine_set_on_closed(
    ten_engine_t *self, ten_engine_on_closed_func_t on_closed,
    void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_close(ten_engine_t *self);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_timer_closed(ten_timer_t *timer,
                                                        void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_extension_context_closed(
    ten_extension_context_t *extension_context, void *on_closed_data);
