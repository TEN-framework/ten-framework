//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/common.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_ADDON_LOADER_SIGNATURE 0xAE4FCDE7983727E4U
#define TEN_ADDON_LOADER_SINGLETON_STORE_SIGNATURE 0x8B3F2A9C7D1E6054U

typedef struct ten_addon_loader_t ten_addon_loader_t;
typedef struct ten_addon_host_t ten_addon_host_t;

typedef struct ten_addon_loader_singleton_store_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_list_t store;  // ten_addon_loader_t
} ten_addon_loader_singleton_store_t;

#define TEN_ADDON_LOADER_SINGLETON_STORE_INIT_VAL              \
  (ten_addon_loader_singleton_store_t) {                       \
    TEN_ADDON_LOADER_SINGLETON_STORE_SIGNATURE,                \
        TEN_SANITIZER_THREAD_CHECK_INIT_VAL, TEN_LIST_INIT_VAL \
  }

#define TEN_ADDON_LOADER_SINGLETON_STORE_INIT(var)     \
  do {                                                 \
    (var) = TEN_ADDON_LOADER_SINGLETON_STORE_INIT_VAL; \
    ten_addon_loader_singleton_store_init(&(var));     \
  } while (0)

typedef void (*ten_addon_loader_on_init_func_t)(
    ten_addon_loader_t *addon_loader, ten_env_t *ten_env);

typedef void (*ten_addon_loader_on_deinit_func_t)(
    ten_addon_loader_t *addon_loader, ten_env_t *ten_env);

typedef void (*ten_addon_loader_on_load_addon_func_t)(
    ten_addon_loader_t *addon_loader, ten_env_t *ten_env,
    TEN_ADDON_TYPE addon_type, const char *addon_name, void *context);

typedef void (*ten_addon_loader_on_init_done_func_t)(ten_env_t *ten_env,
                                                     void *cb_data);

typedef void (*ten_addon_loader_on_deinit_done_func_t)(ten_env_t *ten_env,
                                                       void *cb_data);

typedef void (*ten_addon_loader_on_load_addon_done_func_t)(ten_env_t *ten_env,
                                                           void *cb_data);

typedef struct ten_addon_loader_load_addon_ctx_t {
  ten_addon_loader_on_load_addon_done_func_t cb;
  void *cb_data;
} ten_addon_loader_load_addon_ctx_t;

typedef struct ten_addon_loader_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_addon_host_t *addon_host;

  ten_addon_loader_on_init_func_t on_init;
  ten_addon_loader_on_deinit_func_t on_deinit;
  ten_addon_loader_on_load_addon_func_t on_load_addon;

  ten_addon_loader_on_init_done_func_t on_init_done;
  void *on_init_done_data;

  ten_addon_loader_on_deinit_done_func_t on_deinit_done;
  void *on_deinit_done_data;

  ten_env_t *ten_env;
} ten_addon_loader_t;

TEN_RUNTIME_API bool ten_addon_loader_check_integrity(ten_addon_loader_t *self,
                                                      bool check_thread);

TEN_RUNTIME_API ten_env_t *ten_addon_loader_get_ten_env(
    ten_addon_loader_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_addon_loader_singleton_store_check_integrity(
    ten_addon_loader_singleton_store_t *store, bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_singleton_store_init(
    ten_addon_loader_singleton_store_t *store);

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_singleton_store_deinit(
    ten_addon_loader_singleton_store_t *store);

TEN_RUNTIME_API ten_addon_loader_t *ten_addon_loader_create(
    ten_addon_loader_on_init_func_t on_init,
    ten_addon_loader_on_deinit_func_t on_deinit,
    ten_addon_loader_on_load_addon_func_t on_load_addon);

TEN_RUNTIME_API void ten_addon_loader_destroy(ten_addon_loader_t *self);

TEN_RUNTIME_PRIVATE_API void ten_addon_loader_load_addon(
    ten_addon_loader_t *self, TEN_ADDON_TYPE addon_type, const char *addon_name,
    ten_addon_loader_on_load_addon_done_func_t cb, void *cb_data);

TEN_RUNTIME_PRIVATE_API bool ten_addon_loader_on_load_addon_done(
    ten_env_t *ten_env, void *context);
