//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_manager.h"

#include "include_internal/ten_runtime/binding/nodejs/addon/addon.h"
#include "include_internal/ten_runtime/binding/nodejs/addon/addon_manager.h"
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "ten_runtime/addon/extension/extension.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct addon_manager_register_single_addon_ctx_t {
  ten_nodejs_addon_manager_t *addon_manager_bridge;
  ten_string_t *addon_name;
  void *register_ctx;
  ten_event_t *completed;

  // These fields will be set on the JS main thread.
  ten_addon_t *c_addon;
  ten_nodejs_addon_t *addon_bridge;
} addon_manager_register_single_addon_ctx_t;

static addon_manager_register_single_addon_ctx_t *
addon_manager_register_single_addon_ctx_create(
    ten_nodejs_addon_manager_t *addon_manager_bridge, ten_string_t *addon_name,
    void *register_ctx) {
  addon_manager_register_single_addon_ctx_t *ctx =
      TEN_MALLOC(sizeof(addon_manager_register_single_addon_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_manager_bridge = addon_manager_bridge;
  ctx->addon_name = addon_name;
  ctx->register_ctx = register_ctx;
  ctx->completed = ten_event_create(0, 1);

  ctx->c_addon = NULL;
  ctx->addon_bridge = NULL;

  return ctx;
}

static void addon_manager_register_single_addon_ctx_destroy(
    addon_manager_register_single_addon_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Should not happen.");

  ten_event_destroy(ctx->completed);
  TEN_FREE(ctx);
}

static void ten_nodejs_addon_create_and_attach_callbacks(
    napi_env env, ten_nodejs_addon_t *addon_bridge) {
  TEN_ASSERT(
      addon_bridge && ten_nodejs_addon_check_integrity(addon_bridge, true),
      "Should not happen.");

  napi_value js_addon = NULL;
  napi_status status = napi_get_reference_value(
      env, addon_bridge->bridge.js_instance_ref, &js_addon);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_addon != NULL,
                      "Failed to get JS addon instance.");

  napi_value js_on_init_proxy =
      ten_nodejs_get_property(env, js_addon, "onInitProxy");
  CREATE_JS_CB_TSFN(addon_bridge->js_on_init, env, "[TSFN] addon::onInit",
                    js_on_init_proxy, ten_nodejs_invoke_addon_js_on_init);

  napi_value js_on_deinit_proxy =
      ten_nodejs_get_property(env, js_addon, "onDeinitProxy");
  CREATE_JS_CB_TSFN(addon_bridge->js_on_deinit, env, "[TSFN] addon::onDeinit",
                    js_on_deinit_proxy, ten_nodejs_invoke_addon_js_on_deinit);

  napi_value js_on_create_instance_proxy =
      ten_nodejs_get_property(env, js_addon, "onCreateInstanceProxy");
  CREATE_JS_CB_TSFN(addon_bridge->js_on_create_instance, env,
                    "[TSFN] addon::onCreateInstance",
                    js_on_create_instance_proxy,
                    ten_nodejs_invoke_addon_js_on_create_instance);
}

static bool ten_nodejs_addon_manager_check_integrity(
    ten_nodejs_addon_manager_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      TEN_NODEJS_ADDON_MANAGER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void invoke_addon_manager_js_register_single_addon(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  addon_manager_register_single_addon_ctx_t *ctx = data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_nodejs_addon_manager_t *addon_manager_bridge = ctx->addon_manager_bridge;
  TEN_ASSERT(addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                         addon_manager_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call registerSingleAddon() of the TEN JS addon manager.

    // Get the TEN JS addon manager.
    napi_value js_addon_manager = NULL;
    status = napi_get_reference_value(
        env, addon_manager_bridge->bridge.js_instance_ref, &js_addon_manager);
    GOTO_LABEL_IF_NAPI_FAIL(error,
                            status == napi_ok && js_addon_manager != NULL,
                            "Failed to get JS addon manager: %d", status);

    // Call registerSingleAddon().

    napi_value result = NULL;

    napi_value js_addon_name = NULL;
    status = napi_create_string_utf8(
        env, ten_string_get_raw_str(ctx->addon_name),
        ten_string_len(ctx->addon_name), &js_addon_name);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_addon_name != NULL,
                            "Failed to create JS addon name: %d", status);

    napi_value js_context = NULL;
    status = napi_create_external(env, ctx, NULL, NULL, &js_context);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_context != NULL,
                            "Failed to create JS context: %d", status);

    napi_value argv[] = {js_addon_name, js_context};
    status = napi_call_function(env, js_addon_manager, fn, 2, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(
        error, status == napi_ok,
        "Failed to call JS addon manager registerSingleAddon(): %d", status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS addon manager registerSingleAddon().");

done:
  ten_event_set(ctx->completed);
}

static void ten_nodejs_addon_manager_create_and_attach_callbacks(
    napi_env env, ten_nodejs_addon_manager_t *addon_manager_bridge) {
  TEN_ASSERT(addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                         addon_manager_bridge, true),
             "Should not happen.");

  napi_value js_addon_manager = NULL;
  napi_status status = napi_get_reference_value(
      env, addon_manager_bridge->bridge.js_instance_ref, &js_addon_manager);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_addon_manager != NULL,
                      "Failed to get JS addon manager instance.");

  napi_value js_register_single_addon =
      ten_nodejs_get_property(env, js_addon_manager, "registerSingleAddon");
  CREATE_JS_CB_TSFN(addon_manager_bridge->js_register_single_addon, env,
                    "[TSFN] addon_manager::registerSingleAddon",
                    js_register_single_addon,
                    invoke_addon_manager_js_register_single_addon);
}

static void ten_nodejs_addon_manager_detach_callbacks(
    ten_nodejs_addon_manager_t *self) {
  TEN_ASSERT(self && ten_nodejs_addon_manager_check_integrity(self, true),
             "Should not happen.");

  /**
   * The addon manager holds references to its TSFN, and it's time to drop the
   * references.
   */
  ten_nodejs_tsfn_dec_rc(self->js_register_single_addon);
}

static void ten_nodejs_addon_manager_release_js_tsfn(
    napi_env env, ten_nodejs_addon_manager_t *self) {
  TEN_ASSERT(
      env && self && ten_nodejs_addon_manager_check_integrity(self, true),
      "Should not happen.");

  ten_nodejs_tsfn_release(self->js_register_single_addon);
}

static void ten_nodejs_addon_manager_destroy(ten_nodejs_addon_manager_t *self) {
  TEN_ASSERT(self && ten_nodejs_addon_manager_check_integrity(self, true),
             "Should not happen.");

  ten_nodejs_addon_manager_detach_callbacks(self);
  ten_sanitizer_thread_check_deinit(&self->thread_check);
  TEN_FREE(self);
}

static void ten_nodejs_addon_manager_finalize(napi_env env, void *data,
                                              TEN_UNUSED void *hint) {
  TEN_LOGI("TEN JS addon manager is finalized.");

  ten_nodejs_addon_manager_t *addon_manager_bridge = data;
  TEN_ASSERT(addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                         addon_manager_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  status =
      napi_delete_reference(env, addon_manager_bridge->bridge.js_instance_ref);
  TEN_ASSERT(status == napi_ok,
             "Failed to delete JS addon manager reference: %d", status);

  addon_manager_bridge->bridge.js_instance_ref = NULL;

  ten_nodejs_addon_manager_destroy(addon_manager_bridge);
}

static napi_value ten_nodejs_addon_manager_create(napi_env env,
                                                  napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 1;
  napi_value argv[1];  // this
  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_nodejs_addon_manager_t *addon_manager_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_addon_manager_t));
  TEN_ASSERT(addon_manager_bridge,
             "Failed to allocate memory for addon manager bridge.");

  ten_signature_set(&addon_manager_bridge->signature,
                    TEN_NODEJS_ADDON_MANAGER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(
      &addon_manager_bridge->thread_check);

  // Wraps the native bridge instance ('addon_manager_bridge') in the javascript
  // addon manager object (args[0]). And the returned reference ('js_ref' here)
  // is a weak reference, meaning it has a reference count of 0.
  napi_status status = napi_wrap(env, argv[0], addon_manager_bridge,
                                 ten_nodejs_addon_manager_finalize, NULL,
                                 &addon_manager_bridge->bridge.js_instance_ref);
  GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                          "Failed to bind JS addon manager & bridge: %d",
                          status);

  ten_nodejs_addon_manager_create_and_attach_callbacks(env,
                                                       addon_manager_bridge);

  goto done;

error:
  if (addon_manager_bridge) {
    TEN_FREE(addon_manager_bridge);
  }

done:
  return js_undefined(env);
}

static napi_value ten_nodejs_addon_manager_register_addon_as_extension(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  TEN_LOGD(
      "TEN JS Addon: ten_nodejs_addon_manager_register_addon_as_extension");

  ten_string_t addon_name;
  TEN_STRING_INIT(addon_name);

  // name, addon_instance, addon_manager_register_single_addon_ctx_t
  const size_t argc = 3;
  napi_value argv[3];

  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  if (!ten_nodejs_get_str_from_js(env, argv[0], &addon_name)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to get addon name from JS string.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  TEN_LOGI("Registering addon: %s", ten_string_get_raw_str(&addon_name));

  ten_nodejs_addon_t *addon_bridge = NULL;
  napi_status status = napi_unwrap(env, argv[1], (void **)&addon_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && addon_bridge != NULL,
                                "Failed to get addon bridge: %d", status);

  void *register_ctx = NULL;
  status = napi_get_value_external(env, argv[2], &register_ctx);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && register_ctx != NULL,
                                "Failed to get register context: %d", status);

  ten_string_copy(&addon_bridge->addon_name, &addon_name);

  ten_nodejs_addon_create_and_attach_callbacks(env, addon_bridge);

  addon_manager_register_single_addon_ctx_t *ctx = register_ctx;
  TEN_ASSERT(ctx, "Should not happen.");

  ctx->c_addon = &addon_bridge->c_addon;
  ctx->addon_bridge = addon_bridge;
  TEN_ASSERT(ctx->c_addon && ctx->addon_bridge, "Should not happen.");

  ten_string_deinit(&addon_name);

  return js_undefined(env);
}

static void ten_nodejs_addon_register_func(TEN_UNUSED TEN_ADDON_TYPE addon_type,
                                           ten_string_t *addon_name,
                                           void *register_ctx_,
                                           void *user_data) {
  ten_addon_register_ctx_t *register_ctx = register_ctx_;
  TEN_ASSERT(register_ctx, "Should not happen.");

  ten_app_t *app = register_ctx->app;
  TEN_ASSERT(app && ten_app_check_integrity(app, true), "Should not happen.");

  // Call the static JS function AddonManager._register_single_addon from the
  // app thread other than the JS main thread.
  ten_nodejs_addon_manager_t *addon_manager_bridge = user_data;
  TEN_ASSERT(
      addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                  addon_manager_bridge,
                                  // NOLINTNEXTLINE(thread-check)
                                  // thread-check: The ownership of the
                                  // addon_manager_bridge is the JS main thread.
                                  false),
      "Should not happen.");

  addon_manager_register_single_addon_ctx_t *ctx =
      addon_manager_register_single_addon_ctx_create(addon_manager_bridge,
                                                     addon_name, register_ctx);
  TEN_ASSERT(ctx, "Should not happen.");

  bool rc = ten_nodejs_tsfn_invoke(
      addon_manager_bridge->js_register_single_addon, ctx);
  TEN_ASSERT(rc, "Failed to invoke JS addon manager registerSingleAddon().");

  // Currently, addon register phase 1 and phase 2 are both synchronous, and
  // C++/Go/Python addon registrations can be completed synchronously. However,
  // Node.js addon registration needs to switch to the JS main thread to execute
  // (Node.js limitation), so we need to wait for the JS main thread to complete
  // the actions required for phase 2 before continuing.
  //
  // TODO(xilin): This event wait can be removed in the future by making the
  // `ten_addon_register` function asynchronous (i.e., making phase 2
  // asynchronous). Currently, we need to wait for the JS thread to complete the
  // registration before proceeding.
  ten_event_wait(ctx->completed, -1);

  ten_addon_t *addon_instance = ctx->c_addon;
  TEN_ASSERT(addon_instance, "Should not happen.");

  ten_nodejs_addon_t *addon_bridge = ctx->addon_bridge;
  TEN_ASSERT(addon_bridge, "Should not happen.");

  ten_addon_host_t *c_addon_host = ten_addon_register_extension(
      ten_string_get_raw_str(addon_name), NULL, addon_instance, register_ctx);
  TEN_ASSERT(c_addon_host, "ten_addon_register_extension() failed.");

  addon_bridge->c_addon_host = c_addon_host;

  TEN_LOGD("JS addon manager registerSingleAddon() completed.");

  addon_manager_register_single_addon_ctx_destroy(ctx);
}

static napi_value ten_nodejs_addon_manager_add_extension_addon(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  TEN_LOGD("TEN JS Addon: ten_nodejs_addon_manager_add_extension_addon");

  ten_string_t addon_name;
  TEN_STRING_INIT(addon_name);

  const size_t argc = 2;
  napi_value argv[2];  // addon_manager, name
  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_addon_manager_t *addon_manager_bridge = NULL;
  napi_status status =
      napi_unwrap(env, argv[0], (void **)&addon_manager_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && addon_manager_bridge != NULL,
      "Failed to get addon manager bridge: %d", status);
  TEN_ASSERT(addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                         addon_manager_bridge, true),
             "Should not happen.");

  if (!ten_nodejs_get_str_from_js(env, argv[1], &addon_name)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to get addon name from JS string.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  TEN_LOGI("Adding Nodejs addon: %s to the native addon manager.",
           ten_string_get_raw_str(&addon_name));

  ten_error_t error;
  TEN_ERROR_INIT(error);

  ten_addon_manager_t *addon_manager = ten_addon_manager_get_instance();

  bool rc = ten_addon_manager_add_addon(
      addon_manager, "extension", ten_string_get_raw_str(&addon_name),
      ten_nodejs_addon_register_func, addon_manager_bridge, &error);
  if (!rc) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to add addon to the native addon manager.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_deinit(&error);
  ten_string_deinit(&addon_name);

  return js_undefined(env);
}

static napi_value ten_nodejs_addon_manager_deinit(napi_env env,
                                                  napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  TEN_LOGD("TEN JS Addon: ten_nodejs_addon_manager_deinit");

  const size_t argc = 1;
  napi_value argv[1];  // this
  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_addon_manager_t *addon_manager_bridge = NULL;
  napi_status status =
      napi_unwrap(env, argv[0], (void **)&addon_manager_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && addon_manager_bridge != NULL,
      "Failed to get addon manager bridge: %d", status);
  TEN_ASSERT(addon_manager_bridge && ten_nodejs_addon_manager_check_integrity(
                                         addon_manager_bridge, true),
             "Should not happen.");

  // From now on, the JS callback(s) are useless, so release them all.
  ten_nodejs_addon_manager_release_js_tsfn(env, addon_manager_bridge);

  return js_undefined(env);
}

napi_value ten_nodejs_addon_manager_module_init(napi_env env,
                                                napi_value exports) {
  TEN_ASSERT(env && exports, "Should not happen.");

  EXPORT_FUNC(env, exports, ten_nodejs_addon_manager_create);
  EXPORT_FUNC(env, exports,
              ten_nodejs_addon_manager_register_addon_as_extension);
  EXPORT_FUNC(env, exports, ten_nodejs_addon_manager_add_extension_addon);
  EXPORT_FUNC(env, exports, ten_nodejs_addon_manager_deinit);
  return exports;
}