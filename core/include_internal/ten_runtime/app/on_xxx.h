//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/app/app.h"

typedef struct ten_protocol_t ten_protocol_t;
typedef struct ten_addon_loader_t ten_addon_loader_t;

typedef struct ten_app_thread_on_addon_create_protocol_done_ctx_t {
  ten_protocol_t *protocol;
  ten_addon_context_t *addon_context;
} ten_app_thread_on_addon_create_protocol_done_ctx_t;

typedef struct ten_app_thread_on_addon_create_addon_loader_done_ctx_t {
  ten_addon_loader_t *addon_loader;
  ten_addon_context_t *addon_context;
} ten_app_thread_on_addon_create_addon_loader_done_ctx_t;

TEN_RUNTIME_PRIVATE_API ten_app_thread_on_addon_create_protocol_done_ctx_t *
ten_app_thread_on_addon_create_protocol_done_ctx_create(void);

TEN_RUNTIME_PRIVATE_API void ten_app_thread_on_addon_create_protocol_done(
    void *self, void *arg);

TEN_RUNTIME_PRIVATE_API ten_app_thread_on_addon_create_addon_loader_done_ctx_t *
ten_app_thread_on_addon_create_addon_loader_done_ctx_create(void);

TEN_RUNTIME_PRIVATE_API void ten_app_thread_on_addon_create_addon_loader_done(
    void *self, void *arg);

TEN_RUNTIME_PRIVATE_API bool ten_app_on_ten_env_proxy_released(ten_env_t *self);
