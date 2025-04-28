//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "ten_runtime/addon/addon.h"

typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_addon_store_t ten_addon_store_t;
typedef struct ten_addon_loader_t ten_addon_loader_t;

typedef void (*ten_addon_loader_on_all_singleton_instances_created_cb_t)(
    ten_env_t *ten_env, void *cb_data);

typedef void (*ten_addon_loader_on_all_singleton_instances_destroyed_cb_t)(
    ten_env_t *ten_env, void *cb_data);

typedef struct ten_addon_loader_on_all_singleton_instances_created_ctx_t {
  ten_env_t *ten_env;
  size_t desired_count;
  ten_addon_loader_on_all_singleton_instances_created_cb_t cb;
  void *cb_data;
} ten_addon_loader_on_all_singleton_instances_created_ctx_t;

typedef struct ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t {
  ten_env_t *ten_env;
  ten_addon_loader_on_all_singleton_instances_destroyed_cb_t cb;
  void *cb_data;
} ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t;

typedef struct ten_app_on_addon_loader_init_done_ctx_t {
  ten_addon_loader_t *addon_loader;
  void *cb_data;
} ten_app_on_addon_loader_init_done_ctx_t;

typedef struct ten_app_on_addon_loader_deinit_done_ctx_t {
  ten_addon_loader_t *addon_loader;
  void *cb_data;
} ten_app_on_addon_loader_deinit_done_ctx_t;

TEN_RUNTIME_API ten_addon_host_t *ten_addon_register_addon_loader(
    const char *name, const char *base_dir, ten_addon_t *addon,
    void *register_ctx);

TEN_RUNTIME_PRIVATE_API void ten_addon_unregister_all_addon_loader(
    ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API bool ten_addon_create_addon_loader(
    ten_env_t *ten_env, const char *addon_name, const char *instance_name,
    ten_env_addon_create_instance_done_cb_t cb, void *cb_data,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_addons_create_singleton_instance(
    ten_env_t *ten_env,
    ten_addon_loader_on_all_singleton_instances_created_cb_t cb, void *cb_data);

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_destroy_all_singleton_instances(
    ten_env_t *ten_env,
    ten_addon_loader_on_all_singleton_instances_destroyed_cb_t cb,
    void *cb_data);
