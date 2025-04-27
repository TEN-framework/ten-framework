//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/addon/addon_manager.h"

typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_addon_store_t ten_addon_store_t;
typedef struct ten_addon_manager_t ten_addon_manager_t;

#define TEN_REGISTER_ADDON_AS_PROTOCOL(PROTOCOL_NAME, ADDON) \
  TEN_ADDON_REGISTER(protocol, PROTOCOL_NAME, ADDON)

typedef void (*ten_env_addon_on_create_protocol_async_cb_t)(
    ten_env_t *ten_env, ten_protocol_t *protocol, void *cb_data);

typedef struct ten_addon_create_protocol_ctx_t {
  ten_string_t uri;
  TEN_PROTOCOL_ROLE role;
  ten_env_addon_on_create_protocol_async_cb_t cb;
  void *user_data;
} ten_addon_create_protocol_ctx_t;

TEN_RUNTIME_PRIVATE_API bool ten_addon_create_protocol_with_uri(
    ten_env_t *ten_env, const char *uri, TEN_PROTOCOL_ROLE role,
    ten_env_addon_on_create_protocol_async_cb_t cb, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_addon_create_protocol(
    ten_env_t *ten_env, const char *addon_name, const char *instance_name,
    TEN_PROTOCOL_ROLE role, ten_env_addon_on_create_protocol_async_cb_t cb,
    void *user_data, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_addon_protocol_find(
    ten_env_t *ten_env, const char *protocol);

TEN_RUNTIME_API ten_addon_host_t *ten_addon_register_protocol(
    const char *name, const char *base_dir, ten_addon_t *addon,
    void *register_ctx);

TEN_RUNTIME_PRIVATE_API void ten_addon_unregister_all_protocol(
    ten_env_t *ten_env);
