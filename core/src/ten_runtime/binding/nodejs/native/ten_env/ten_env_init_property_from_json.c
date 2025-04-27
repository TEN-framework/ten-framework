//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_notify_init_property_from_json_ctx_t {
  ten_string_t json_str;
  ten_nodejs_tsfn_t *js_cb;
} ten_env_notify_init_property_from_json_ctx_t;

typedef struct ten_nodejs_init_property_from_json_call_ctx_t {
  ten_nodejs_tsfn_t *js_cb;
  ten_error_t *error;
} ten_nodejs_init_property_from_json_call_ctx_t;

static ten_env_notify_init_property_from_json_ctx_t *
ten_env_notify_init_property_from_json_ctx_create(ten_nodejs_tsfn_t *js_cb) {
  ten_env_notify_init_property_from_json_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_init_property_from_json_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  TEN_STRING_INIT(ctx->json_str);
  ctx->js_cb = js_cb;

  return ctx;
}

static void ten_env_notify_init_property_from_json_ctx_destroy(
    ten_env_notify_init_property_from_json_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->json_str);
  ctx->js_cb = NULL;

  TEN_FREE(ctx);
}

static void tsfn_proxy_init_property_from_json_callback(napi_env env,
                                                        napi_value js_cb,
                                                        void *context,
                                                        void *data) {
  ten_nodejs_init_property_from_json_call_ctx_t *call_info =
      (ten_nodejs_init_property_from_json_call_ctx_t *)data;
  TEN_ASSERT(call_info, "Should not happen.");

  napi_value js_error = NULL;

  if (call_info->error) {
    js_error = ten_nodejs_create_error(env, call_info->error);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  } else {
    js_error = js_undefined(env);
  }

  napi_value argv[] = {js_error};
  napi_value result = NULL;
  napi_status status =
      napi_call_function(env, js_error, js_cb, 1, argv, &result);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to call JS callback", NULL);

  if (call_info->error) {
    ten_error_destroy(call_info->error);
  }

  ten_nodejs_tsfn_release(call_info->js_cb);

  TEN_FREE(call_info);
}

static void ten_env_proxy_notify_init_property_from_json(ten_env_t *ten_env,
                                                         void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_init_property_from_json_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_nodejs_tsfn_t *js_cb = ctx->js_cb;
  TEN_ASSERT(js_cb && ten_nodejs_tsfn_check_integrity(js_cb, false),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_init_property_from_json(
      ten_env, ten_string_get_raw_str(&ctx->json_str), &err);

  ten_error_t *cloned_error = NULL;
  if (!rc) {
    cloned_error = ten_error_create();
    ten_error_copy(cloned_error, &err);
  }

  ten_nodejs_init_property_from_json_call_ctx_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_init_property_from_json_call_ctx_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->js_cb = js_cb;
  call_info->error = cloned_error;

  rc = ten_nodejs_tsfn_invoke(js_cb, call_info);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  ten_env_notify_init_property_from_json_ctx_destroy(ctx);
}

napi_value ten_nodejs_ten_env_init_property_from_json(napi_env env,
                                                      napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 3;
  napi_value args[argc];  // ten_env, json_str, callback

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
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  if (ten_env_bridge->c_ten_env_proxy == NULL) {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.initPropertyFromJson() failed because ten is closed.");

    napi_value js_error = ten_nodejs_create_error(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    ten_error_deinit(&err);

    return js_error;
  }

  ten_nodejs_tsfn_t *cb_tsfn = ten_nodejs_tsfn_create(
      env, "[TSFN] TenEnv::initPropertyFromJson callback", args[2],
      tsfn_proxy_init_property_from_json_callback);
  RETURN_UNDEFINED_IF_NAPI_FAIL(cb_tsfn, "Failed to create TSFN.");

  ten_env_notify_init_property_from_json_ctx_t *notify_info =
      ten_env_notify_init_property_from_json_ctx_create(cb_tsfn);
  TEN_ASSERT(notify_info, "Failed to create notify info.");

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &notify_info->json_str);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get JSON string from JS.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  rc = ten_env_proxy_notify(ten_env_bridge->c_ten_env_proxy,
                            ten_env_proxy_notify_init_property_from_json,
                            notify_info, false, &err);
  if (!rc) {
    napi_value js_error = ten_nodejs_create_error(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    ten_env_notify_init_property_from_json_ctx_destroy(notify_info);

    // The JS callback will not be called, so we need to clean up the tsfn.
    ten_nodejs_tsfn_release(cb_tsfn);

    ten_error_deinit(&err);

    return js_error;
  }

  ten_error_deinit(&err);

  return js_null(env);
}
