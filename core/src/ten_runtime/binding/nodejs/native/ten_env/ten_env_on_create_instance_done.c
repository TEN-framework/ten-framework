//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/nodejs/extension/extension.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_nodejs_ten_env_on_create_instance_done_ctx_t {
  ten_addon_host_t *addon_host;
  void *instance;
  void *context;
} ten_nodejs_ten_env_on_create_instance_done_ctx_t;

static void ten_app_addon_host_on_create_instance_done(void *from, void *args) {
  ten_app_t *app = (ten_app_t *)from;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_nodejs_ten_env_on_create_instance_done_ctx_t *ctx =
      (ten_nodejs_ten_env_on_create_instance_done_ctx_t *)args;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_addon_host_t *addon_host = ctx->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_create_instance_done(addon_host->ten_env, ctx->instance,
                                            ctx->context, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  TEN_FREE(ctx);
}

napi_value ten_nodejs_ten_env_on_create_instance_done(napi_env env,
                                                      napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 3;
  napi_value args[argc];  // ten_env, instance, context
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_ten_env_t *ten_env_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&ten_env_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && ten_env_bridge != NULL,
                                "Failed to get rte bridge: %d", status);
  TEN_ASSERT(ten_env_bridge, "Should not happen.");
  TEN_ASSERT(ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  ten_nodejs_extension_t *extension_bridge = NULL;
  status = napi_unwrap(env, args[1], (void **)&extension_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && extension_bridge != NULL,
                                "Failed to get extension bridge: %d", status);

  TEN_ASSERT(extension_bridge, "Should not happen.");
  TEN_ASSERT(ten_nodejs_extension_check_integrity(extension_bridge, true),
             "Should not happen.");

  void *context = NULL;
  status = napi_get_value_external(env, args[2], &context);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && context != NULL,
                                "Failed to get context: %d", status);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_t *c_ten_env = ten_env_bridge->c_ten_env;
  TEN_ASSERT(c_ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(c_ten_env, false), "Should not happen.");

  TEN_ASSERT(c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(c_ten_env);
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, false),
             "Should not happen.");

  ten_app_t *app = addon_host->attached_app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Should not happen.");

  ten_nodejs_ten_env_on_create_instance_done_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_nodejs_ten_env_on_create_instance_done_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_host = addon_host;
  ctx->instance = extension_bridge->c_extension;
  ctx->context = context;

  int post_task_rc = ten_runloop_post_task_tail(
      ten_app_get_attached_runloop(app),
      ten_app_addon_host_on_create_instance_done, app, ctx);
  TEN_ASSERT(post_task_rc == 0, "Failed to post task.");

  ten_error_deinit(&err);

  ten_binding_handle_set_me_in_target_lang((ten_binding_handle_t *)c_ten_env,
                                           NULL);
  // Release the reference to the JS ten_env object.
  uint32_t js_ten_env_ref_count = 0;
  status = napi_reference_unref(env, ten_env_bridge->bridge.js_instance_ref,
                                &js_ten_env_ref_count);

  return js_undefined(env);
}
