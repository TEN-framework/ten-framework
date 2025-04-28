//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

ten_addon_host_t *ten_addon_register_addon_loader(const char *name,
                                                  const char *base_dir,
                                                  ten_addon_t *addon,
                                                  void *register_ctx) {
  return ten_addon_register(TEN_ADDON_TYPE_ADDON_LOADER, name, base_dir, addon,
                            register_ctx);
}

void ten_addon_unregister_all_addon_loader(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_store_t *addon_store = &app->addon_loader_store;
  TEN_ASSERT(addon_store, "Should not happen.");
  TEN_ASSERT(ten_addon_store_check_integrity(addon_store, true),
             "Should not happen.");

  ten_addon_store_del_all(addon_store);
}

// This function is called in the app thread, so `ten_env` is the app's
// `ten_env`.
bool ten_addon_create_addon_loader(ten_env_t *ten_env, const char *addon_name,
                                   const char *instance_name,
                                   ten_env_addon_create_instance_done_cb_t cb,
                                   void *cb_data, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(addon_name && instance_name, "Should not happen.");

  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  // This function is called in the app thread, so `ten_env` is the app's
  // `ten_env`.
  if (ten_app_thread_call_by_me(app)) {
    ten_addon_context_t *addon_context = ten_addon_context_create();
    TEN_ASSERT(addon_context, "Failed to create addon context.");

    ten_addon_context_set_creation_info(
        addon_context, TEN_ADDON_TYPE_ADDON_LOADER, addon_name, instance_name);

    addon_context->flow = TEN_ADDON_CONTEXT_FLOW_APP_CREATE_ADDON_LOADER;
    addon_context->flow_target.app = app;
    addon_context->create_instance_done_cb = cb;
    addon_context->create_instance_done_cb_data = cb_data;

    bool rc = ten_addon_create_instance_async(ten_env, addon_context);
    if (!rc) {
      ten_addon_context_destroy(addon_context);
    }

    return rc;
  } else {
    TEN_ASSERT(0, "Should not happen.");
    return true;
  }
}
