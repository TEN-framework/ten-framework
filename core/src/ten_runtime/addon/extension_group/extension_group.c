//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/check.h"

ten_addon_host_t *ten_addon_register_extension_group(const char *name,
                                                     const char *base_dir,
                                                     ten_addon_t *addon,
                                                     void *register_ctx) {
  return ten_addon_register(TEN_ADDON_TYPE_EXTENSION_GROUP, name, base_dir,
                            addon, register_ctx);
}

bool ten_addon_create_extension_group(
    ten_env_t *ten_env, const char *addon_name, const char *instance_name,
    ten_env_addon_create_instance_done_cb_t cb, void *user_data) {
  TEN_ASSERT(addon_name && instance_name, "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ENGINE,
             "Should not happen.");

  // Because there might be more than one engine (threads) to create extension
  // groups from the corresponding extension group addons simultaneously. So we
  // can _not_ save the pointers of 'cb' and 'user_data' into 'ten_env', they
  // will override each other. Instead we need to pass those pointers through
  // parameters below.

  ten_engine_t *engine = ten_env_get_attached_engine(ten_env);
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  TEN_ASSERT(addon_context, "Failed to allocate memory.");

  ten_addon_context_set_creation_info(
      addon_context, TEN_ADDON_TYPE_EXTENSION_GROUP, addon_name, instance_name);

  addon_context->flow = TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_EXTENSION_GROUP;
  addon_context->flow_target.engine = engine;
  addon_context->create_instance_done_cb = cb;
  addon_context->create_instance_done_cb_data = user_data;

  bool rc = ten_addon_create_instance_async(ten_env, addon_context);
  if (!rc) {
    ten_addon_context_destroy(addon_context);
  }

  return rc;
}

bool ten_addon_destroy_extension_group(
    ten_env_t *ten_env, ten_extension_group_t *extension_group,
    ten_env_addon_destroy_instance_done_cb_t cb, void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(cb, "Should not happen.");
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  ten_addon_host_t *addon_host = extension_group->addon_host;
  TEN_ASSERT(addon_host,
             "Should not happen, otherwise, memory leakage will occur.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  addon_context->flow =
      TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION_GROUP;
  addon_context->flow_target.extension_thread =
      extension_group->extension_thread;
  addon_context->destroy_instance_done_cb = cb;
  addon_context->destroy_instance_done_cb_data = cb_data;

  return ten_addon_host_destroy_instance_async(addon_host, extension_group,
                                               addon_context);
}

void ten_addon_unregister_all_extension_group(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_store_t *addon_store = &app->extension_group_store;
  TEN_ASSERT(addon_store, "Should not happen.");
  TEN_ASSERT(ten_addon_store_check_integrity(addon_store, true),
             "Should not happen.");

  ten_addon_store_del_all(addon_store);
}
