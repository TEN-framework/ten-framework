//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/binding/common.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/signature.h"

#define TEN_ADDON_SIGNATURE 0xDB9CA797E07377D4U

typedef struct ten_app_t ten_app_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;

typedef void (*ten_env_addon_create_instance_done_cb_t)(ten_env_t *ten_env,
                                                        void *instance,
                                                        void *cb_data);

typedef void (*ten_env_addon_destroy_instance_done_cb_t)(ten_env_t *ten_env,
                                                         void *cb_data);

typedef void (*ten_env_on_all_addons_unregistered_cb_t)(ten_env_t *ten_env,
                                                        void *cb_data);

typedef enum TEN_ADDON_CONTEXT_FLOW {
  TEN_ADDON_CONTEXT_FLOW_INVALID,

  TEN_ADDON_CONTEXT_FLOW_APP_CREATE_PROTOCOL,
  TEN_ADDON_CONTEXT_FLOW_APP_CREATE_ADDON_LOADER,

  TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_EXTENSION_GROUP,
  TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_PROTOCOL,

  TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_CREATE_EXTENSION,
  TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION,
  TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION_GROUP,
} TEN_ADDON_CONTEXT_FLOW;

typedef struct ten_addon_context_t {
  TEN_ADDON_TYPE addon_type;
  ten_string_t addon_name;
  ten_string_t instance_name;

  TEN_ADDON_CONTEXT_FLOW flow;
  union {
    ten_app_t *app;
    ten_engine_t *engine;
    ten_extension_thread_t *extension_thread;
  } flow_target;

  ten_env_addon_create_instance_done_cb_t create_instance_done_cb;
  void *create_instance_done_cb_data;

  ten_env_addon_destroy_instance_done_cb_t destroy_instance_done_cb;
  void *destroy_instance_done_cb_data;
} ten_addon_context_t;

typedef struct ten_addon_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;

  ten_addon_on_init_func_t on_init;
  ten_addon_on_deinit_func_t on_deinit;

  ten_addon_on_create_instance_func_t on_create_instance;
  ten_addon_on_destroy_instance_func_t on_destroy_instance;

  ten_addon_on_destroy_func_t on_destroy;

  void *user_data;
} ten_addon_t;

typedef struct ten_addon_on_create_extension_instance_ctx_t {
  ten_string_t addon_name;
  ten_string_t instance_name;
  TEN_ADDON_TYPE addon_type;  // Used to retrieve addon from the correct store.
  ten_env_addon_create_instance_done_cb_t cb;
  void *cb_data;
} ten_addon_on_create_extension_instance_ctx_t;

TEN_RUNTIME_API bool ten_addon_check_integrity(ten_addon_t *self);

TEN_RUNTIME_PRIVATE_API TEN_ADDON_TYPE
ten_addon_type_from_string(const char *addon_type_str);

TEN_RUNTIME_PRIVATE_API ten_addon_t *ten_addon_unregister(
    ten_addon_store_t *store, const char *addon_name);

TEN_RUNTIME_API void ten_addon_unregister_all_and_cleanup_after_app_close(
    ten_env_t *ten_env, ten_env_on_all_addons_unregistered_cb_t cb,
    void *cb_data);

TEN_RUNTIME_PRIVATE_API ten_addon_store_t *ten_addon_get_store(void);

TEN_RUNTIME_PRIVATE_API bool ten_addon_create_instance_async(
    ten_env_t *ten_env, ten_addon_context_t *addon_context);

TEN_RUNTIME_API const char *ten_addon_type_to_string(TEN_ADDON_TYPE type);

TEN_RUNTIME_PRIVATE_API ten_addon_context_t *ten_addon_context_create(void);

TEN_RUNTIME_PRIVATE_API void ten_addon_context_set_creation_info(
    ten_addon_context_t *self, TEN_ADDON_TYPE addon_type,
    const char *addon_name, const char *instance_name);

TEN_RUNTIME_PRIVATE_API void ten_addon_context_destroy(
    ten_addon_context_t *self);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_addon_register(
    TEN_ADDON_TYPE addon_type, const char *name, const char *base_dir,
    ten_addon_t *addon, void *register_ctx);

TEN_RUNTIME_API void ten_addon_init(
    ten_addon_t *self, ten_addon_on_init_func_t on_init,
    ten_addon_on_deinit_func_t on_deinit,
    ten_addon_on_create_instance_func_t on_create_instance,
    ten_addon_on_destroy_instance_func_t on_destroy_instance,
    ten_addon_on_destroy_func_t on_destroy);
