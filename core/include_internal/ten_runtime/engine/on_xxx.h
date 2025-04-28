//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;
typedef struct ten_protocol_t ten_protocol_t;

typedef struct ten_engine_thread_on_addon_create_protocol_done_ctx_t {
  ten_protocol_t *protocol;
  ten_addon_context_t *addon_context;
} ten_engine_thread_on_addon_create_protocol_done_ctx_t;

TEN_RUNTIME_PRIVATE_API void ten_engine_on_extension_thread_closed_task(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_remove_extension_thread_from_engine(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_addon_create_extension_group_done(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API ten_engine_thread_on_addon_create_protocol_done_ctx_t *
ten_engine_thread_on_addon_create_protocol_done_ctx_create(void);

TEN_RUNTIME_PRIVATE_API void ten_engine_thread_on_addon_create_protocol_done(
    void *self, void *arg);
