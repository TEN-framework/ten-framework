//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/addon/extension/extension.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/on_xxx.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static ten_addon_on_create_extension_instance_ctx_t *
ten_addon_on_create_extension_instance_ctx_create(
    TEN_ADDON_TYPE addon_type, const char *addon_name,
    const char *instance_name, ten_env_addon_create_instance_done_cb_t cb,
    void *cb_data) {
  TEN_ASSERT(addon_name && instance_name, "Should not happen.");

  ten_addon_on_create_extension_instance_ctx_t *self =
      (ten_addon_on_create_extension_instance_ctx_t *)TEN_MALLOC(
          sizeof(ten_addon_on_create_extension_instance_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_string_init_formatted(&self->addon_name, "%s", addon_name);
  ten_string_init_formatted(&self->instance_name, "%s", instance_name);
  self->addon_type = addon_type;
  self->cb = cb;
  self->cb_data = cb_data;

  return self;
}

void ten_addon_on_create_extension_instance_ctx_destroy(
    ten_addon_on_create_extension_instance_ctx_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_string_deinit(&self->addon_name);
  ten_string_deinit(&self->instance_name);

  TEN_FREE(self);
}

ten_addon_create_extension_done_ctx_t *
ten_addon_create_extension_done_ctx_create(
    const char *extension_name,
    ten_extension_group_create_extensions_done_ctx_t *ctx) {
  TEN_ASSERT(extension_name && ctx, "Invalid argument.");

  ten_addon_create_extension_done_ctx_t *self =
      TEN_MALLOC(sizeof(ten_addon_create_extension_done_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_string_init_from_c_str_with_size(&self->extension_name, extension_name,
                                       strlen(extension_name));
  self->create_extensions_done_ctx = ctx;

  return self;
}

void ten_addon_create_extension_done_ctx_destroy(
    ten_addon_create_extension_done_ctx_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_string_deinit(&self->extension_name);

  TEN_FREE(self);
}

bool ten_addon_create_extension(ten_env_t *ten_env, const char *addon_name,
                                const char *instance_name,
                                ten_env_addon_create_instance_done_cb_t cb,
                                void *cb_data, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(addon_name && instance_name, "Should not happen.");

  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  // Because there might be more than one extension threads to create extensions
  // from the corresponding extension addons simultaneously. So we can _not_
  // save the pointers of 'cb' and 'user_data' into 'ten_env', they will
  // override each other. Instead we need to pass those pointers through
  // parameters below.

  ten_extension_group_t *extension_group =
      ten_env_get_attached_extension_group(ten_env);
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  // Check whether current thread is extension thread. If not, we should switch
  // to extension thread.
  if (ten_extension_thread_call_by_me(extension_group->extension_thread)) {
    ten_addon_context_t *addon_context = ten_addon_context_create();
    TEN_ASSERT(addon_context, "Failed to allocate memory.");

    ten_addon_context_set_creation_info(addon_context, TEN_ADDON_TYPE_EXTENSION,
                                        addon_name, instance_name);

    addon_context->flow =
        TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_CREATE_EXTENSION;
    addon_context->flow_target.extension_thread =
        extension_group->extension_thread;
    addon_context->create_instance_done_cb = cb;
    addon_context->create_instance_done_cb_data = cb_data;

    bool rc = ten_addon_create_instance_async(ten_env, addon_context);
    if (!rc) {
      ten_addon_context_destroy(addon_context);
    }

    return rc;
  } else {
    ten_addon_on_create_extension_instance_ctx_t *ctx =
        ten_addon_on_create_extension_instance_ctx_create(
            TEN_ADDON_TYPE_EXTENSION, addon_name, instance_name, cb, cb_data);

    int rc = ten_runloop_post_task_tail(
        ten_extension_group_get_attached_runloop(extension_group),
        ten_extension_thread_create_extension_instance,
        extension_group->extension_thread, ctx);
    TEN_ASSERT(!rc, "Should not happen.");

    return true;
  }
}

bool ten_addon_destroy_extension(ten_env_t *ten_env, ten_extension_t *extension,
                                 ten_env_addon_destroy_instance_done_cb_t cb,
                                 void *cb_data, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  TEN_ASSERT(cb, "Should not happen.");

  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  ten_addon_host_t *addon_host = extension->addon_host;
  TEN_ASSERT(addon_host,
             "Should not happen, otherwise, memory leakage will occur.");

  ten_extension_group_t *extension_group =
      ten_env_get_attached_extension_group(ten_env);
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  if (ten_extension_thread_call_by_me(extension_group->extension_thread)) {
    ten_addon_context_t *addon_context = ten_addon_context_create();
    addon_context->flow =
        TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION;
    addon_context->flow_target.extension_thread =
        extension_group->extension_thread;
    addon_context->destroy_instance_done_cb = cb;
    addon_context->destroy_instance_done_cb_data = cb_data;

    return ten_addon_host_destroy_instance_async(addon_host, extension,
                                                 addon_context);
  } else {
    ten_addon_host_on_destroy_instance_ctx_t *destroy_instance_info =
        ten_addon_host_on_destroy_instance_ctx_create(addon_host, extension, cb,
                                                      cb_data);

    int rc = ten_runloop_post_task_tail(
        ten_extension_group_get_attached_runloop(extension_group),
        ten_extension_thread_destroy_addon_instance,
        extension_group->extension_thread, destroy_instance_info);
    TEN_ASSERT(!rc, "Should not happen.");

    return true;
  }
}

ten_addon_host_t *ten_addon_register_extension(const char *name,
                                               const char *base_dir,
                                               ten_addon_t *addon,
                                               void *register_ctx) {
  return ten_addon_register(TEN_ADDON_TYPE_EXTENSION, name, base_dir, addon,
                            register_ctx);
}

void ten_addon_unregister_all_extension(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_store_t *addon_store = &app->extension_store;
  TEN_ASSERT(addon_store, "Should not happen.");
  TEN_ASSERT(ten_addon_store_check_integrity(addon_store, true),
             "Should not happen.");

  ten_addon_store_del_all(addon_store);
}
