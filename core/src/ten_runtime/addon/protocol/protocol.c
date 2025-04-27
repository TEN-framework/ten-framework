//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/protocol/protocol.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/ten_env/log.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uri.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_object.h"

ten_addon_host_t *ten_addon_register_protocol(const char *name,
                                              const char *base_dir,
                                              ten_addon_t *addon,
                                              void *register_ctx) {
  return ten_addon_register(TEN_ADDON_TYPE_PROTOCOL, name, base_dir, addon,
                            register_ctx);
}

static bool ten_addon_protocol_match_protocol(ten_addon_host_t *self,
                                              const char *protocol) {
  TEN_ASSERT(self && self->type == TEN_ADDON_TYPE_PROTOCOL && protocol,
             "Should not happen.");

  ten_value_t *manifest = &self->manifest;
  TEN_ASSERT(manifest, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(manifest), "Invalid use of manifest %p.",
             manifest);

  bool found = false;
  ten_list_t *addon_protocols =
      ten_value_object_peek_array(manifest, TEN_STR_PROTOCOL);
  TEN_ASSERT(addon_protocols, "Should not happen.");

  ten_list_foreach (addon_protocols, iter) {
    ten_value_t *addon_protocol_value = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(
        addon_protocol_value && ten_value_check_integrity(addon_protocol_value),
        "Should not happen.");

    const char *addon_protocol =
        ten_value_peek_raw_str(addon_protocol_value, NULL);
    if (!strcmp(addon_protocol, protocol)) {
      found = true;
      break;
    }
  }

  return found;
}

ten_addon_host_t *ten_addon_protocol_find(ten_env_t *ten_env,
                                          const char *protocol) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(protocol, "Should not happen.");

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");
  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_host_t *result = NULL;

  ten_addon_store_t *store = &app->protocol_store;
  TEN_ASSERT(store, "Should not happen.");
  TEN_ASSERT(ten_addon_store_check_integrity(store, true),
             "Should not happen.");

  ten_list_foreach (&store->store, iter) {
    ten_addon_host_t *addon = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon && addon->type == TEN_ADDON_TYPE_PROTOCOL,
               "Should not happen.");

    if (!ten_addon_protocol_match_protocol(addon, protocol)) {
      continue;
    }

    result = addon;
    break;
  }

  return result;
}

static ten_addon_create_protocol_ctx_t *ten_addon_create_protocol_ctx_create(
    const char *uri, TEN_PROTOCOL_ROLE role,
    ten_env_addon_on_create_protocol_async_cb_t cb, void *user_data) {
  TEN_ASSERT(role > TEN_PROTOCOL_ROLE_INVALID, "Should not happen.");

  ten_addon_create_protocol_ctx_t *ctx =
      (ten_addon_create_protocol_ctx_t *)TEN_MALLOC(
          sizeof(ten_addon_create_protocol_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  if (!uri || strlen(uri) == 0) {
    TEN_STRING_INIT(ctx->uri);
  } else {
    ten_string_init_formatted(&ctx->uri, "%s", uri);
  }

  ctx->role = role;
  ctx->cb = cb;
  ctx->user_data = user_data;

  return ctx;
}

static void ten_addon_create_protocol_ctx_destroy(
    ten_addon_create_protocol_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Should not happen.");

  ten_string_deinit(&ctx->uri);

  TEN_FREE(ctx);
}

static void proxy_on_addon_protocol_created(ten_env_t *ten_env, void *instance,
                                            void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_addon_create_protocol_ctx_t *ctx =
      (ten_addon_create_protocol_ctx_t *)cb_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_protocol_t *protocol = instance;
  if (protocol) {
    if (!ten_string_is_empty(&ctx->uri)) {
      ten_string_set_formatted(&protocol->uri, "%s",
                               ten_string_get_raw_str(&ctx->uri));
    }

    protocol->role = ctx->role;
  }

  if (ctx->cb) {
    ctx->cb(ten_env, protocol, ctx->user_data);
  }

  ten_addon_create_protocol_ctx_destroy(ctx);
}

static void ten_app_create_protocol_with_uri(ten_app_t *self,
                                             ten_addon_context_t *ctx) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_addon_create_protocol_ctx_t *create_protocol_ctx =
      (ten_addon_create_protocol_ctx_t *)ctx->create_instance_done_cb_data;
  TEN_ASSERT(create_protocol_ctx, "Should not happen.");

  const char *uri = ten_string_get_raw_str(&create_protocol_ctx->uri);
  TEN_ASSERT(uri, "Invalid argument.");

  ten_env_t *ten_env = ten_app_get_ten_env(self);
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_string_t *protocol_str = ten_uri_get_protocol(uri);
  TEN_ASSERT(protocol_str, "Should not happen.");

  ten_addon_host_t *addon_host =
      ten_addon_protocol_find(ten_env, ten_string_get_raw_str(protocol_str));
  if (!addon_host) {
    TEN_ENV_LOG_ERROR_INTERNAL(
        ten_env,
        "Failed to handle protocol '%s' because no addon installed for it",
        uri);

    // Set the addon name and instance name to empty strings to indicate we
    // cannot find the corresponding addon according to the uri.
    ten_addon_context_set_creation_info(ctx, TEN_ADDON_TYPE_PROTOCOL, "", "");
  } else {
    ten_addon_context_set_creation_info(
        ctx, TEN_ADDON_TYPE_PROTOCOL, ten_string_get_raw_str(&addon_host->name),
        ten_string_get_raw_str(&addon_host->name));

    TEN_ENV_LOG_INFO_INTERNAL(ten_env, "Loading protocol addon: %s",
                              ten_string_get_raw_str(&addon_host->name));
  }

  ten_string_destroy(protocol_str);

  bool rc = ten_addon_create_instance_async(ten_env, ctx);
  TEN_ASSERT(rc, "Should not happen.");
}

static void ten_app_create_protocol_with_uri_task(void *self_, void *arg) {
  ten_app_t *self = (ten_app_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid argument.");

  ten_addon_context_t *ctx = (ten_addon_context_t *)arg;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_app_create_protocol_with_uri(self, ctx);
}

bool ten_addon_create_protocol_with_uri(
    ten_env_t *ten_env, const char *uri, TEN_PROTOCOL_ROLE role,
    ten_env_addon_on_create_protocol_async_cb_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(uri && role > TEN_PROTOCOL_ROLE_INVALID, "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(err, "Invalid argument.");

  TEN_ENV_ATTACH_TO attach_to = ten_env_get_attach_to(ten_env);
  if (attach_to != TEN_ENV_ATTACH_TO_APP &&
      attach_to != TEN_ENV_ATTACH_TO_ENGINE) {
    TEN_ENV_LOG_ERROR_INTERNAL(ten_env, "Invalid ten_env attach_to: %d",
                               attach_to);

    ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, "Invalid ten_env.");
    return false;
  }

  // We have to switch to the addon manager app thread to find the addon host
  // according to the URI. If we find one, we can use it to create a protocol
  // instance.
  //
  // If we don't find one, for now we will callback with NULL to indicate the
  // failure. In the future, if the protocol is also dynamically loaded, we will
  // try to load the protocol addon and find the addon host again.

  ten_app_t *addon_manager_app =
      ten_addon_manager_get_belonging_app(ten_addon_manager_get_instance());
  TEN_ASSERT(addon_manager_app, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function could be called from the engine thread or other
  // app threads.
  TEN_ASSERT(ten_app_check_integrity(addon_manager_app, false),
             "Should not happen.");

  ten_addon_create_protocol_ctx_t *ctx =
      ten_addon_create_protocol_ctx_create(uri, role, cb, user_data);
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  TEN_ASSERT(addon_context, "Failed to allocate memory.");

  if (attach_to == TEN_ENV_ATTACH_TO_APP) {
    addon_context->flow = TEN_ADDON_CONTEXT_FLOW_APP_CREATE_PROTOCOL;
    addon_context->flow_target.app = ten_env_get_attached_app(ten_env);
  } else {
    addon_context->flow = TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_PROTOCOL;
    addon_context->flow_target.engine = ten_env_get_attached_engine(ten_env);
  }
  addon_context->create_instance_done_cb = proxy_on_addon_protocol_created;
  addon_context->create_instance_done_cb_data = ctx;

  if (ten_app_thread_call_by_me(addon_manager_app)) {
    ten_app_create_protocol_with_uri(addon_manager_app, addon_context);
  } else {
    int rc = ten_runloop_post_task_tail(
        ten_app_get_attached_runloop(addon_manager_app),
        ten_app_create_protocol_with_uri_task, addon_manager_app,
        addon_context);
    TEN_ASSERT(!rc, "Should not happen.");
  }

  return true;
}

bool ten_addon_create_protocol(ten_env_t *ten_env, const char *addon_name,
                               const char *instance_name,
                               TEN_PROTOCOL_ROLE role,
                               ten_env_addon_on_create_protocol_async_cb_t cb,
                               void *user_data, ten_error_t *err) {
  TEN_ASSERT(addon_name && instance_name, "Should not happen.");
  TEN_ASSERT(role > TEN_PROTOCOL_ROLE_INVALID, "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  TEN_ENV_ATTACH_TO attach_to = ten_env_get_attach_to(ten_env);
  if (attach_to != TEN_ENV_ATTACH_TO_APP &&
      attach_to != TEN_ENV_ATTACH_TO_ENGINE) {
    TEN_LOGE("Invalid ten_env attach_to: %d", attach_to);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, "Invalid ten_env.");
    }
    return false;
  }

  TEN_LOGD("Loading protocol addon: %s", addon_name);

  ten_addon_create_protocol_ctx_t *ctx =
      ten_addon_create_protocol_ctx_create(NULL, role, cb, user_data);
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  TEN_ASSERT(addon_context, "Failed to allocate memory.");

  ten_addon_context_set_creation_info(addon_context, TEN_ADDON_TYPE_PROTOCOL,
                                      addon_name, instance_name);

  if (attach_to == TEN_ENV_ATTACH_TO_APP) {
    addon_context->flow = TEN_ADDON_CONTEXT_FLOW_APP_CREATE_PROTOCOL;
    addon_context->flow_target.app = ten_env_get_attached_app(ten_env);
  } else {
    addon_context->flow = TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_PROTOCOL;
    addon_context->flow_target.engine = ten_env_get_attached_engine(ten_env);
  }
  addon_context->create_instance_done_cb = proxy_on_addon_protocol_created;
  addon_context->create_instance_done_cb_data = ctx;

  bool rc = ten_addon_create_instance_async(ten_env, addon_context);

  if (!rc) {
    TEN_LOGE("Failed to create protocol for %s", addon_name);

    ten_addon_context_destroy(addon_context);
    ten_addon_create_protocol_ctx_destroy(ctx);

    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to create protocol for addon: %s.", addon_name);
    }

    return false;
  }

  return true;
}

void ten_addon_unregister_all_protocol(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_store_t *addon_store = &app->protocol_store;
  TEN_ASSERT(addon_store, "Should not happen.");

  ten_addon_store_del_all(addon_store);
}
