//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

#define TEN_ADDON_HOST_SIGNATURE 0x44FAE6B3F920A44EU

typedef struct ten_addon_t ten_addon_t;
typedef struct ten_addon_store_t ten_addon_store_t;

typedef struct ten_addon_host_t {
  ten_signature_t signature;

  ten_string_t name;  // The name of the addon.
  ten_string_t base_dir;

  ten_value_t manifest;
  ten_value_t property;

  ten_metadata_info_t *manifest_info;
  ten_metadata_info_t *property_info;

  ten_addon_t *addon;
  ten_addon_store_t *store;

  ten_ref_t ref;  // Used to control the timing of addon destruction.
  ten_env_t *ten_env;

  TEN_ADDON_TYPE type;

  ten_app_t *attached_app;
} ten_addon_host_t;

typedef struct ten_addon_host_on_destroy_instance_ctx_t {
  ten_addon_host_t *addon_host;
  void *instance;
  ten_env_addon_destroy_instance_done_cb_t cb;
  void *cb_data;
} ten_addon_host_on_destroy_instance_ctx_t;

TEN_RUNTIME_PRIVATE_API void ten_addon_host_init(ten_addon_host_t *self);

TEN_RUNTIME_API void ten_addon_host_destroy(ten_addon_host_t *self);

TEN_RUNTIME_PRIVATE_API void ten_addon_host_find_and_set_base_dir(
    ten_addon_host_t *self, const char *path);

TEN_RUNTIME_PRIVATE_API bool ten_addon_host_destroy_instance_async(
    ten_addon_host_t *self, void *instance, ten_addon_context_t *addon_context);

TEN_RUNTIME_PRIVATE_API void ten_addon_host_create_instance_async(
    ten_addon_host_t *self, const char *name,
    ten_addon_context_t *addon_context);

TEN_RUNTIME_PRIVATE_API bool ten_addon_host_destroy_instance(
    ten_addon_host_t *self, ten_env_t *ten_env, void *instance);

/**
 * @brief The base directory of the loaded addon. This function can be
 * called before any TEN app starts. Note that the returned string must be
 * destroyed by users.
 */
TEN_RUNTIME_PRIVATE_API const char *ten_addon_host_get_base_dir(
    ten_addon_host_t *self);

TEN_RUNTIME_API bool ten_addon_host_check_integrity(ten_addon_host_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_addon_host_get_name(
    ten_addon_host_t *self);

TEN_RUNTIME_PRIVATE_API ten_addon_host_on_destroy_instance_ctx_t *
ten_addon_host_on_destroy_instance_ctx_create(
    ten_addon_host_t *self, void *instance,
    ten_env_addon_destroy_instance_done_cb_t cb, void *cb_data);

TEN_RUNTIME_PRIVATE_API void ten_addon_host_on_destroy_instance_ctx_destroy(
    ten_addon_host_on_destroy_instance_ctx_t *self);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_addon_host_create(
    TEN_ADDON_TYPE type);

TEN_RUNTIME_PRIVATE_API void ten_addon_host_load_metadata(
    ten_addon_host_t *self, ten_env_t *ten_env,
    ten_addon_on_init_func_t on_init);
