//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"

typedef struct ten_extension_thread_on_addon_create_extension_done_ctx_t {
  ten_extension_t *extension;
  ten_addon_context_t *addon_context;
} ten_extension_thread_on_addon_create_extension_done_ctx_t;

TEN_RUNTIME_PRIVATE_API ten_extension_thread_on_addon_create_extension_done_ctx_t *
ten_extension_thread_on_addon_create_extension_done_ctx_create(void);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_addon_create_extension_done_ctx_destroy(
    ten_extension_thread_on_addon_create_extension_done_ctx_t *self);

TEN_RUNTIME_API void ten_extension_inherit_thread_ownership(
    ten_extension_t *self, ten_extension_thread_t *extension_thread);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_extension_group_on_init_done(void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_extension_group_on_deinit_done(void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_on_removed_from_engine(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_on_all_extensions_deleted(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_addon_create_extension_done(void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_addon_destroy_extension_done(void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_create_extension_instance(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_destroy_addon_instance(
    void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_on_addon_destroy_extension_group_done(void *self_,
                                                           void *arg);
