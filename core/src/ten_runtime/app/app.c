//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/app/app.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/app/migration.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/global/global.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"
#include "ten_utils/value/value.h"

static void ten_app_inherit_thread_ownership(ten_app_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The correct threading ownership will be setup soon, so we can
  // _not_ check thread safety here.
  TEN_ASSERT(ten_app_check_integrity(self, false), "Invalid use of app %p.",
             self);

  // Move the ownership of the app relevant resources to the belonging app
  // thread.
  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &self->thread_check);

  ten_sanitizer_thread_check_inherit_from(&self->ten_env->thread_check,
                                          &self->thread_check);

  ten_sanitizer_thread_check_inherit_from(&self->path_table->thread_check,
                                          &self->thread_check);

  ten_sanitizer_thread_check_inherit_from(&self->extension_store.thread_check,
                                          &self->thread_check);
  ten_sanitizer_thread_check_inherit_from(
      &self->extension_group_store.thread_check, &self->thread_check);
  ten_sanitizer_thread_check_inherit_from(&self->protocol_store.thread_check,
                                          &self->thread_check);
  ten_sanitizer_thread_check_inherit_from(
      &self->addon_loader_store.thread_check, &self->thread_check);

  ten_sanitizer_thread_check_inherit_from(
      &self->addon_loader_singleton_store.thread_check, &self->thread_check);
}

static void *ten_app_routine(void *args) {
  ten_app_t *self = (ten_app_t *)args;
  TEN_ASSERT(self, "Invalid argument.");
  if (!self) {
    TEN_LOGE("Invalid app pointer.");
    return NULL;
  }

  // This is the app thread, so transfer the thread ownership of the app to this
  // thread no matter where the app is created.
  ten_app_inherit_thread_ownership(self);

  if (self->run_in_background) {
    ten_event_set(self->belonging_thread_is_set);
  }

  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_LOGI("[%s] App is created", ten_app_get_uri(self));

  self->loop = ten_runloop_create(NULL);
  TEN_ASSERT(self->loop, "Should not happen.");
  if (!self->loop) {
    TEN_LOGE("Failed to create runloop.");
    return NULL;
  }

  // The runloop has been created, and now we can push runloop tasks. Therefore,
  // we check if the app has already been triggered to close here.
  if (ten_app_is_closing(self)) {
    ten_app_close(self, NULL);
  }

  ten_global_add_app(self);

  ten_app_start(self);

  return NULL;
}

ten_app_t *ten_app_create(ten_app_on_configure_func_t on_configure,
                          ten_app_on_init_func_t on_init,
                          ten_app_on_deinit_func_t on_deinit,
                          TEN_UNUSED ten_error_t *err) {
  ten_app_t *self = (ten_app_t *)TEN_MALLOC(sizeof(ten_app_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->on_configure = on_configure;
  self->on_init = on_init;
  self->on_deinit = on_deinit;

  self->binding_handle.me_in_target_lang = self;

  ten_signature_set(&self->signature, (ten_signature_t)TEN_APP_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);
  self->belonging_thread_is_set = ten_event_create(0, 0);

  self->state_lock = ten_mutex_create();
  self->state = TEN_APP_STATE_INIT;

  self->endpoint_protocol = NULL;

  ten_list_init(&self->engines);
  ten_list_init(&self->orphan_connections);

  ten_list_init(&self->predefined_graph_infos);

  ten_value_init_object_with_move(&self->manifest, NULL);
  ten_value_init_object_with_move(&self->property, NULL);
  ten_schema_store_init(&self->schema_store);

  TEN_STRING_INIT(self->uri);

  self->in_msgs_lock = ten_mutex_create();
  ten_list_init(&self->in_msgs);

  self->ten_env = ten_env_create_for_app(self);
  TEN_ASSERT(self->ten_env, "Should not happen.");

  TEN_STRING_INIT(self->base_dir);

  self->path_table = ten_path_table_create(TEN_PATH_TABLE_ATTACH_TO_APP, self);

  self->manifest_info = NULL;
  self->property_info = NULL;

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  ten_service_hub_init(&self->service_hub);
#endif

  TEN_ADDON_STORE_INIT(self->extension_store);
  TEN_ADDON_STORE_INIT(self->extension_group_store);
  TEN_ADDON_STORE_INIT(self->protocol_store);
  TEN_ADDON_STORE_INIT(self->addon_loader_store);

  TEN_ADDON_LOADER_SINGLETON_STORE_INIT(self->addon_loader_singleton_store);

  self->preload_all_addons = false;

  self->is_standalone_test_app = false;
  self->standalone_test_mode = TEN_EXTENSION_TESTER_TEST_MODE_INVALID;
  TEN_STRING_INIT(self->standalone_tested_target_name);

  self->user_data = NULL;

  return self;
}

void ten_app_destroy(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  TEN_LOGD("[%s] Destroy app", ten_app_get_uri(self));

  ten_global_del_app(self);

  ten_signature_set(&self->signature, 0);

  ten_string_deinit(&self->standalone_tested_target_name);

  ten_env_destroy(self->ten_env);
  ten_mutex_destroy(self->state_lock);

  ten_value_deinit(&self->manifest);
  ten_value_deinit(&self->property);

  if (self->manifest_info) {
    ten_metadata_info_destroy(self->manifest_info);
    self->manifest_info = NULL;
  }
  if (self->property_info) {
    ten_metadata_info_destroy(self->property_info);
    self->property_info = NULL;
  }

  ten_schema_store_deinit(&self->schema_store);

  ten_addon_store_deinit(&self->extension_store);
  ten_addon_store_deinit(&self->extension_group_store);
  ten_addon_store_deinit(&self->protocol_store);
  ten_addon_store_deinit(&self->addon_loader_store);

  ten_addon_loader_singleton_store_deinit(&self->addon_loader_singleton_store);

  ten_mutex_destroy(self->in_msgs_lock);
  ten_list_clear(&self->in_msgs);

  ten_runloop_destroy(self->loop);
  self->loop = NULL;

  TEN_ASSERT(ten_list_is_empty(&self->engines), "Should not happen.");
  TEN_ASSERT(ten_list_is_empty(&self->orphan_connections),
             "Should not happen.");

  ten_list_clear(&self->predefined_graph_infos);
  ten_string_deinit(&self->uri);

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_event_destroy(self->belonging_thread_is_set);

  ten_string_deinit(&self->base_dir);

  ten_path_table_destroy(self->path_table);

  TEN_FREE(self);
}

bool ten_app_run(ten_app_t *self, bool run_in_background,
                 TEN_UNUSED ten_error_t *error_ctx) {
  TEN_ASSERT(self, "Invalid argument.");

  self->run_in_background = run_in_background;

  // TEN app might be closed before running.
  if (ten_app_is_closing(self)) {
    TEN_LOGW("Failed to run TEN app, because it has been closed");
    return false;
  }

  if (run_in_background) {
    const char *thread_name = ten_string_get_raw_str(&self->uri);
    if (!thread_name || strlen(thread_name) == 0) {
      thread_name = TEN_STR_APP_THREAD;
    }

    ten_thread_create(thread_name, ten_app_routine, self);
    ten_event_wait(self->belonging_thread_is_set, -1);
  } else {
    ten_app_routine(self);
  }

  return true;
}

bool ten_app_wait(ten_app_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  TEN_LOGD("Wait app thread ends");

  if (self->run_in_background &&
      ten_sanitizer_thread_check_get_belonging_thread(&self->thread_check)) {
    int rc = ten_thread_join(
        ten_sanitizer_thread_check_get_belonging_thread(&self->thread_check),
        -1);

    return rc == 0 ? true : false;
  }

  return true;
}

bool ten_app_thread_call_by_me(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, false), "Should not happen.");

  return ten_thread_equal(NULL, ten_sanitizer_thread_check_get_belonging_thread(
                                    &self->thread_check));
}
