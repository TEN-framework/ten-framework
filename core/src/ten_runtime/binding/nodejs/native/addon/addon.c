//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/addon/addon.h"

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"
#include "include_internal/ten_runtime/binding/nodejs/extension/extension.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "js_native_api.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/sanitizer/thread_check.h"

typedef struct addon_on_xxx_callback_info_t {
  ten_nodejs_addon_t *addon_bridge;
  ten_env_t *ten_env;
} addon_on_xxx_callback_info_t;

typedef struct addon_on_create_instance_callback_ctx_t {
  ten_nodejs_addon_t *addon_bridge;
  ten_env_t *ten_env;
  ten_string_t instance_name;
  void *context;
} addon_on_create_instance_callback_ctx_t;

static addon_on_create_instance_callback_ctx_t *
addon_on_create_instance_callback_ctx_create(ten_nodejs_addon_t *addon_bridge,
                                             ten_env_t *ten_env,
                                             const char *instance_name,
                                             void *context) {
  addon_on_create_instance_callback_ctx_t *ctx =
      TEN_MALLOC(sizeof(addon_on_create_instance_callback_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_bridge = addon_bridge;
  ctx->ten_env = ten_env;
  ten_string_init_from_c_str_with_size(&ctx->instance_name, instance_name,
                                       strlen(instance_name));
  ctx->context = context;

  return ctx;
}

static void addon_on_create_instance_callback_ctx_destroy(
    addon_on_create_instance_callback_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Should not happen.");

  ten_string_deinit(&ctx->instance_name);
  TEN_FREE(ctx);
}

bool ten_nodejs_addon_check_integrity(ten_nodejs_addon_t *self,
                                      bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_NODEJS_ADDON_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_nodejs_addon_detach_callbacks(ten_nodejs_addon_t *self) {
  ten_nodejs_tsfn_dec_rc(self->js_on_init);
  ten_nodejs_tsfn_dec_rc(self->js_on_deinit);
  ten_nodejs_tsfn_dec_rc(self->js_on_create_instance);
}

static void ten_nodejs_addon_destroy(ten_nodejs_addon_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_string_deinit(&self->addon_name);
  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_nodejs_addon_detach_callbacks(self);

  TEN_FREE(self);
}

static void ten_nodejs_addon_finalize(napi_env env, void *data,
                                      TEN_UNUSED void *hint) {
  TEN_LOGI("TEN JS Addon is finalized.");

  ten_nodejs_addon_t *addon_bridge = data;
  TEN_ASSERT(
      addon_bridge && ten_nodejs_addon_check_integrity(addon_bridge, true),
      "Should not happen.");

  napi_status status = napi_ok;

  status = napi_delete_reference(env, addon_bridge->bridge.js_instance_ref);
  TEN_ASSERT(status == napi_ok, "Failed to delete JS addon reference: %d",
             status);

  addon_bridge->bridge.js_instance_ref = NULL;

  ten_nodejs_addon_destroy(addon_bridge);
}

void ten_nodejs_invoke_addon_js_on_init(napi_env env, napi_value fn,
                                        TEN_UNUSED void *context, void *data) {
  addon_on_xxx_callback_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(call_info->addon_bridge && ten_nodejs_addon_check_integrity(
                                            call_info->addon_bridge, true),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge = NULL;
  napi_value js_ten_env = ten_nodejs_ten_env_create_new_js_object_and_wrap(
      env, call_info->ten_env, &ten_env_bridge);
  TEN_ASSERT(js_ten_env, "Should not happen.");

  // Increase the reference count of the JS ten_env object to prevent it from
  // being garbage collected.
  uint32_t js_ten_env_ref_count = 0;
  napi_reference_ref(env, ten_env_bridge->bridge.js_instance_ref,
                     &js_ten_env_ref_count);

  napi_status status = napi_ok;

  {
    // Call on_init() of the TEN JS addon.

    // Get the TEN JS addon.
    napi_value js_addon = NULL;
    status = napi_get_reference_value(
        env, call_info->addon_bridge->bridge.js_instance_ref, &js_addon);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_addon != NULL,
                            "Failed to get JS addon: %d", status);

    // Call on_init().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_addon, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS addon on_init(): %d", status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS addon on_init().");

done:
  TEN_FREE(call_info);
}

void ten_nodejs_invoke_addon_js_on_deinit(napi_env env, napi_value fn,
                                          TEN_UNUSED void *context,
                                          void *data) {
  addon_on_xxx_callback_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(call_info->addon_bridge && ten_nodejs_addon_check_integrity(
                                            call_info->addon_bridge, true),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)call_info->ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_deinit() of the TEN JS addon.

    // Get the TEN JS addon.
    napi_value js_addon = NULL;
    status = napi_get_reference_value(
        env, call_info->addon_bridge->bridge.js_instance_ref, &js_addon);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_addon != NULL,
                            "Failed to get JS addon: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    // Call on_deinit().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_addon, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS addon on_deinit(): %d", status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS addon on_deinit().");

  // Call on_deinit_done() here to release the addon.
  ten_env_on_deinit_done(call_info->ten_env, NULL);

done:
  TEN_FREE(call_info);
}

void ten_nodejs_invoke_addon_js_on_create_instance(napi_env env, napi_value fn,
                                                   TEN_UNUSED void *context,
                                                   void *data) {
  addon_on_create_instance_callback_ctx_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(call_info->addon_bridge && ten_nodejs_addon_check_integrity(
                                            call_info->addon_bridge, true),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)call_info->ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_create_instance() of the TEN JS addon.

    // Get the TEN JS addon.
    napi_value js_addon = NULL;
    status = napi_get_reference_value(
        env, call_info->addon_bridge->bridge.js_instance_ref, &js_addon);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_addon != NULL,
                            "Failed to get JS addon: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    napi_value js_instance_name = NULL;
    status = napi_create_string_utf8(
        env, ten_string_get_raw_str(&call_info->instance_name),
        ten_string_len(&call_info->instance_name), &js_instance_name);
    GOTO_LABEL_IF_NAPI_FAIL(error,
                            status == napi_ok && js_instance_name != NULL,
                            "Failed to create JS instance name: %d", status);

    napi_value js_context = NULL;
    status =
        napi_create_external(env, call_info->context, NULL, NULL, &js_context);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_context != NULL,
                            "Failed to create JS context: %d", status);

    napi_value result = NULL;
    napi_value argv[] = {js_ten_env, js_instance_name, js_context};
    status = napi_call_function(env, js_addon, fn, 3, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS addon on_create_instance(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS addon on_create_instance().");

done:
  addon_on_create_instance_callback_ctx_destroy(call_info);
}

static void proxy_on_init(ten_addon_t *addon, ten_env_t *ten_env) {
  TEN_LOGI("addon proxy_on_init");

  ten_nodejs_addon_t *addon_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)addon);
  TEN_ASSERT(addon_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is called from a standalone
                 // addon registration thread, and only when all the addons are
                 // registered, the RTE world can go on, so it's thread safe.
                 ten_nodejs_addon_check_integrity(addon_bridge, false),
             "Should not happen.");

  addon_on_xxx_callback_info_t *call_info =
      TEN_MALLOC(sizeof(addon_on_xxx_callback_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->addon_bridge = addon_bridge;
  call_info->ten_env = ten_env;

  bool rc = ten_nodejs_tsfn_invoke(addon_bridge->js_on_init, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call addon on_init().");
    TEN_FREE(call_info);

    // Failed to call JS on_init(), so that we need to call on_init_done() here
    // to let RTE runtime proceed.
    ten_env_on_init_done(ten_env, NULL);
  }
}

static void proxy_on_deinit(ten_addon_t *addon, ten_env_t *ten_env) {
  TEN_LOGI("addon proxy_on_deinit");

  ten_nodejs_addon_t *addon_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)addon);
  TEN_ASSERT(addon_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is called from a standalone
                 // addon registration thread, and only when all the addons are
                 // registered, the RTE world can go on, so it's thread safe.
                 ten_nodejs_addon_check_integrity(addon_bridge, false),
             "Should not happen.");

  addon_on_xxx_callback_info_t *call_info =
      TEN_MALLOC(sizeof(addon_on_xxx_callback_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->addon_bridge = addon_bridge;
  call_info->ten_env = ten_env;

  bool rc = ten_nodejs_tsfn_invoke(addon_bridge->js_on_deinit, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call addon on_deinit().");
    TEN_FREE(call_info);

    // Failed to call JS on_deinit(), so that we need to call on_deinit_done()
    // here to let RTE runtime proceed.
    ten_env_on_deinit_done(ten_env, NULL);
  }
}

static void proxy_on_create_instance(ten_addon_t *addon, ten_env_t *ten_env,
                                     const char *name, void *context) {
  TEN_LOGI("addon proxy_on_create_instance name: %s", name);

  ten_nodejs_addon_t *addon_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)addon);
  TEN_ASSERT(addon_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is called from a standalone
                 // addon registration thread, and only when all the addons are
                 // registered, the RTE world can go on, so it's thread safe.
                 ten_nodejs_addon_check_integrity(addon_bridge, false),
             "Should not happen.");

  addon_on_create_instance_callback_ctx_t *call_info =
      addon_on_create_instance_callback_ctx_create(addon_bridge, ten_env, name,
                                                   context);

  bool rc =
      ten_nodejs_tsfn_invoke(addon_bridge->js_on_create_instance, call_info);
  TEN_ASSERT(rc, "Failed to call addon on_create_instance().");
}

static void proxy_on_destroy_instance(ten_addon_t *addon, ten_env_t *ten_env,
                                      void *instance, void *context) {
  TEN_LOGI("addon proxy_on_destroy_instance");

  ten_nodejs_addon_t *addon_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)addon);
  TEN_ASSERT(addon_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_addon_check_integrity(addon_bridge, false),
             "Should not happen.");

  TEN_ASSERT(addon_bridge->c_addon_host->type == TEN_ADDON_TYPE_EXTENSION,
             "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)instance);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_extension_t *extension = extension_bridge->c_extension;
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_addon_host_t *addon_host = extension->addon_host;
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  // Because the extension increases the reference count of the corresponding
  // `addon_host` when it is created, the reference count must be decreased
  // when the extension is destroyed.
  ten_ref_dec_ref(&addon_host->ref);
  extension->addon_host = NULL;

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static napi_value ten_nodejs_addon_create(napi_env env,
                                          napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_nodejs_addon_t *addon_bridge = TEN_MALLOC(sizeof(ten_nodejs_addon_t));
  TEN_ASSERT(addon_bridge, "Failed to allocate memory for addon bridge.");

  ten_signature_set(&addon_bridge->signature, TEN_NODEJS_ADDON_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(
      &addon_bridge->thread_check);

  TEN_STRING_INIT(addon_bridge->addon_name);
  addon_bridge->c_addon_host = NULL;

  napi_status status =
      napi_wrap(env, args[0], addon_bridge, ten_nodejs_addon_finalize, NULL,
                &addon_bridge->bridge.js_instance_ref);
  GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                          "Failed to bind JS addon & bridge: %d", status);

  // Increase the reference count of the JS addon object.
  uint32_t js_addon_ref_count = 0;
  status = napi_reference_ref(env, addon_bridge->bridge.js_instance_ref,
                              &js_addon_ref_count);
  GOTO_LABEL_IF_NAPI_FAIL(
      error, status == napi_ok,
      "Failed to increase the reference count of JS addon: %d", status);

  // Create the underlying TEN C addon.
  ten_addon_init(&addon_bridge->c_addon, proxy_on_init, proxy_on_deinit,
                 proxy_on_create_instance, proxy_on_destroy_instance, NULL);

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)&addon_bridge->c_addon, addon_bridge);

  goto done;

error:
  if (addon_bridge) {
    TEN_FREE(addon_bridge);
  }

done:
  return js_undefined(env);
}

static void ten_nodejs_addon_release_js_on_xxx_tsfn(
    napi_env env, ten_nodejs_addon_t *addon_bridge) {
  TEN_ASSERT(env && addon_bridge, "Should not happen.");

  ten_nodejs_tsfn_release(addon_bridge->js_on_init);
  ten_nodejs_tsfn_release(addon_bridge->js_on_deinit);
  ten_nodejs_tsfn_release(addon_bridge->js_on_create_instance);
}

static napi_value ten_nodejs_addon_on_end_of_life(napi_env env,
                                                  napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_addon_t *addon_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&addon_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && addon_bridge != NULL,
                                "Failed to get addon bridge: %d", status);
  TEN_ASSERT(
      addon_bridge && ten_nodejs_addon_check_integrity(addon_bridge, true),
      "Should not happen.");

  // From now on, the JS on_xxx callback(s) are useless, so release them all.
  ten_nodejs_addon_release_js_on_xxx_tsfn(env, addon_bridge);

  // Decrease the reference count of the JS addon object.
  uint32_t js_addon_ref_count = 0;
  status = napi_reference_unref(env, addon_bridge->bridge.js_instance_ref,
                                &js_addon_ref_count);

  return js_undefined(env);
}

napi_value ten_nodejs_addon_module_init(napi_env env, napi_value exports) {
  TEN_ASSERT(env && exports, "Should not happen.");

  EXPORT_FUNC(env, exports, ten_nodejs_addon_create);
  EXPORT_FUNC(env, exports, ten_nodejs_addon_on_end_of_life);
  return exports;
}
