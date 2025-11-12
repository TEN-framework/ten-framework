//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/test/extension_tester.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_runtime/app/service_hub/service_hub.h"
#endif

#define TEN_APP_SIGNATURE 0xF4551554E1E1240EU

typedef struct ten_connection_t ten_connection_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_protocol_t ten_protocol_t;
typedef struct ten_path_table_t ten_path_table_t;
// The symbols for these two below are written in Rust code, so the naming
// convention is a bit different.
typedef struct ServiceHub ServiceHub;
typedef struct MetricHandle MetricHandle;

typedef enum TEN_APP_STATE {
  TEN_APP_STATE_INIT,

  // on_configure() is called.
  TEN_APP_STATE_ON_CONFIGURE,

  // on_configure_done() is called.
  TEN_APP_STATE_ON_CONFIGURE_DONE,

  // on_init() is called.
  TEN_APP_STATE_ON_INIT,

  // on_init_done() is called.
  TEN_APP_STATE_ON_INIT_DONE,

  // The overall closing flow is started.
  TEN_APP_STATE_CLOSING,

  // on_deinit_done() is called.
  TEN_APP_STATE_ON_DEINIT_DONE,
} TEN_APP_STATE;

typedef struct ten_app_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_mutex_t *state_lock;
  TEN_APP_STATE state;

  // @{
  // The accessing of this group of queues are all in the app thread, so no
  // need to apply any synchronization method on it.
  ten_list_t engines;
  ten_list_t orphan_connections;
  // @}

  bool run_in_background;

  // In some situations (for example, in the TEN nodejs environment, in order
  // not to blocking the JS main thread), we need to create a new thread to run
  // TEN app (run_in_background == true). And in the original thread, we need to
  // wait until we get the information of the actual thread of the TEN app, so
  // that if we call app_wait() in the original thread, the behavior would be
  // correct.
  //
  // Below is a pseudo code of this situation:
  // ====
  // app = new App();
  // app->run(true);
  // app->wait();
  ten_event_t *belonging_thread_is_set;

  ten_runloop_t *loop;

  ten_protocol_t *endpoint_protocol;

  ten_env_t *ten_env;

  ten_value_t manifest;
  ten_value_t property;

  ten_metadata_info_t *manifest_info;
  ten_metadata_info_t *property_info;

  ten_list_t predefined_graph_infos;  // ten_predefined_graph_info_t*

  ten_app_on_configure_func_t on_configure;
  ten_app_on_init_func_t on_init;
  ten_app_on_deinit_func_t on_deinit;

  // @{
  // Used to send messages to the app.
  ten_mutex_t *in_msgs_lock;
  ten_list_t in_msgs;
  // @}

  // @{
  // This section is for app default properties, we extract them from app
  // property store for efficiency.
  ten_string_t uri;
  bool one_event_loop_per_engine;
  bool long_running_mode;
  // @}

  ten_schema_store_t schema_store;
  ten_string_t base_dir;

  ten_path_table_t *path_table;

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  ten_service_hub_t service_hub;
#endif

  ten_addon_store_t extension_store;
  ten_addon_store_t extension_group_store;
  ten_addon_store_t protocol_store;
  ten_addon_store_t addon_loader_store;

  ten_addon_loader_singleton_store_t addon_loader_singleton_store;

  // Default value is false, meaning addons are loaded on-demand - if not used,
  // they won't be loaded.
  // If set to true, all addons will be preloaded during app startup by
  // scanning directories, which saves loading time when used later in graphs
  // (e.g., Python module imports and C++ dynamic library loading).
  bool preload_all_addons;

  bool is_standalone_test_app;
  TEN_EXTENSION_TESTER_TEST_MODE standalone_test_mode;
  ten_string_t standalone_tested_target_name;

  void *user_data;
} ten_app_t;

TEN_RUNTIME_PRIVATE_API void ten_app_add_orphan_connection(
    ten_app_t *self, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API void ten_app_del_orphan_connection(
    ten_app_t *self, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API bool ten_app_has_orphan_connection(
    ten_app_t *self, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API void ten_app_start(ten_app_t *self);

TEN_RUNTIME_API ten_sanitizer_thread_check_t *ten_app_get_thread_check(
    ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_thread_call_by_me(ten_app_t *self);

TEN_RUNTIME_API ten_runloop_t *ten_app_get_attached_runloop(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_app_get_uri(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API void ten_app_on_configure(ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_app_on_init(ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_app_on_init_done(ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_app_on_deinit(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_app_on_deinit_done(ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API void ten_app_on_configure_done(ten_env_t *ten_env);
