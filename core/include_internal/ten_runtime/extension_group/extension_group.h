//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "ten_runtime/binding/common.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_EXTENSION_GROUP_SIGNATURE 0x94F72EDA6137DF04U

typedef struct ten_extension_t ten_extension_t;
typedef struct ten_extension_context_t ten_extension_context_t;
typedef struct ten_extension_group_t ten_extension_group_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_metadata_info_t ten_metadata_info_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_env_t ten_env_t;
typedef struct ten_extension_group_info_t ten_extension_group_info_t;

typedef void (*ten_extension_group_on_configure_func_t)(
    ten_extension_group_t *self, ten_env_t *ten_env);

typedef void (*ten_extension_group_on_init_func_t)(ten_extension_group_t *self,
                                                   ten_env_t *ten_env);

typedef void (*ten_extension_group_on_deinit_func_t)(
    ten_extension_group_t *self, ten_env_t *ten_env);

typedef void (*ten_extension_group_on_create_extensions_func_t)(
    ten_extension_group_t *self, ten_env_t *ten_env);

typedef void (*ten_extension_group_on_destroy_extensions_func_t)(
    ten_extension_group_t *self, ten_env_t *ten_env, ten_list_t extensions);

typedef enum TEN_EXTENSION_GROUP_STATE {
  TEN_EXTENSION_GROUP_STATE_INIT,

  TEN_EXTENSION_GROUP_STATE_DEINIT,       // on_deinit() is started.
  TEN_EXTENSION_GROUP_STATE_DEINIT_DONE,  // on_deinit_done() is called.
} TEN_EXTENSION_GROUP_STATE;

typedef struct ten_extension_group_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_EXTENSION_GROUP_STATE state;

  // @{
  // Public interface
  ten_extension_group_on_configure_func_t on_configure;
  ten_extension_group_on_init_func_t on_init;
  ten_extension_group_on_deinit_func_t on_deinit;
  ten_extension_group_on_create_extensions_func_t on_create_extensions;
  ten_extension_group_on_destroy_extensions_func_t on_destroy_extensions;
  // @}

  ten_app_t *app;
  ten_extension_context_t *extension_context;
  ten_extension_thread_t *extension_thread;

  ten_env_t *ten_env;

  ten_addon_host_t *addon_host;
  ten_string_t name;

  ten_extension_group_info_t *extension_group_info;

  // ten_extension_addon_and_instance_name_pair_t
  ten_list_t extension_addon_and_instance_name_pairs;

  // The error encountered in the extension thread before completing all
  // initializations (i.e., before starting the lifecycle stages of all included
  // extensions). For example, a failure to create extensions.
  ten_error_t err_before_ready;

  ten_value_t manifest;
  ten_value_t property;

  ten_metadata_info_t *manifest_info;
  ten_metadata_info_t *property_info;

  size_t extensions_cnt_of_being_destroyed;
} ten_extension_group_t;

TEN_RUNTIME_API bool ten_extension_group_check_integrity(
    ten_extension_group_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_extension_group_t *ten_extension_group_create(
    const char *name, ten_extension_group_on_configure_func_t on_configure,
    ten_extension_group_on_init_func_t on_init,
    ten_extension_group_on_deinit_func_t on_deinit,
    ten_extension_group_on_create_extensions_func_t on_create_extensions,
    ten_extension_group_on_destroy_extensions_func_t on_destroy_extensions);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_destroy(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_extension_group_get_ten_env(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_create_extensions(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_destroy_extensions(
    ten_extension_group_t *self, ten_list_t extensions);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_set_addon(
    ten_extension_group_t *self, ten_addon_host_t *addon_host);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *
ten_extension_group_create_cmd_result_for_invalid_dest(
    ten_shared_ptr_t *origin_cmd, ten_string_t *target_group_name);

TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_extension_group_get_attached_runloop(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_init_done(ten_env_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_extension_group_on_deinit_done(
    ten_env_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_destroy_extensions_done(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_group_on_create_extensions_done(
    ten_extension_group_t *self, ten_list_t *extensions);

TEN_RUNTIME_PRIVATE_API ten_list_t *
ten_extension_group_get_extension_addon_and_instance_name_pairs(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_extension_group_set_extension_cnt_of_being_destroyed(
    ten_extension_group_t *self, size_t new_cnt);

TEN_RUNTIME_PRIVATE_API size_t
ten_extension_group_decrement_extension_cnt_of_being_destroyed(
    ten_extension_group_t *self);

TEN_RUNTIME_PRIVATE_API ten_extension_group_t *
ten_extension_group_create_internal(
    const char *name, ten_extension_group_on_configure_func_t on_configure,
    ten_extension_group_on_init_func_t on_init,
    ten_extension_group_on_deinit_func_t on_deinit,
    ten_extension_group_on_create_extensions_func_t on_create_extensions,
    ten_extension_group_on_destroy_extensions_func_t on_destroy_extensions);

TEN_RUNTIME_PRIVATE_API const char *ten_extension_group_get_name(
    ten_extension_group_t *self, bool check_thread);
