//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value_json.h"

static void tsfn_proxy_get_property_to_json_callback(napi_env env,
                                                     napi_value js_cb,
                                                     void *context,
                                                     void *data) {
  ten_nodejs_get_property_call_ctx_t *ctx =
      (ten_nodejs_get_property_call_ctx_t *)data;
  TEN_ASSERT(ctx, "Should not happen.");

  napi_value js_res = NULL;
  napi_value js_error = NULL;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *value = ctx->value;
  if (!value) {
    if (ctx->error) {
      js_error = ten_nodejs_create_error(env, ctx->error);
      ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
    } else {
      ten_error_set(&err, TEN_ERROR_CODE_GENERIC,
                    "Failed to get property value");
      js_error = ten_nodejs_create_error(env, &err);
      ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
    }
  } else {
    ten_json_t value_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    bool success = ten_value_to_json(value, &value_json);
    if (!success) {
      ten_error_set(&err, TEN_ERROR_CODE_GENERIC,
                    "Failed to convert property value to JSON");
      js_error = ten_nodejs_create_error(env, &err);
      ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
    } else {
      bool must_free = false;
      const char *json_str = ten_json_to_string(&value_json, NULL, &must_free);
      TEN_ASSERT(json_str, "Should not happen.");

      ten_json_deinit(&value_json);

      napi_status status =
          napi_create_string_utf8(env, json_str, NAPI_AUTO_LENGTH, &js_res);
      ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create JS string: %d",
                          status);

      if (must_free) {
        TEN_FREE(json_str);
      }
    }
  }

  if (!js_res) {
    js_res = js_undefined(env);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  napi_value args[] = {js_res, js_error};
  napi_value result = NULL;
  napi_status status = napi_call_function(env, js_res, js_cb, 2, args, &result);
  ASSERT_IF_NAPI_FAIL(
      status == napi_ok,
      "Failed to call JS callback of TenEnv::getPropertyToJson: %d", status);

  ten_nodejs_tsfn_release(ctx->cb_tsfn);

  ten_nodejs_get_property_call_ctx_destroy(ctx);
}

napi_value ten_nodejs_ten_env_get_property_to_json(napi_env env,
                                                   napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 3;
  napi_value args[argc];  // ten_env, path, callback
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

    ten_error_set(
        &err, TEN_ERROR_CODE_TEN_IS_CLOSED,
        "ten_env.get_property_to_json() failed because ten is closed.");

    napi_value js_error = ten_nodejs_create_error(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    ten_error_deinit(&err);

    return js_error;
  }

  ten_string_t name;
  TEN_STRING_INIT(name);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property name", NULL);

  ten_nodejs_tsfn_t *cb_tsfn =
      ten_nodejs_tsfn_create(env, "[TSFN] TenEnv::getPropertyToJson callback",
                             args[2], tsfn_proxy_get_property_to_json_callback);
  RETURN_UNDEFINED_IF_NAPI_FAIL(cb_tsfn, "Failed to create TSFN", NULL);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  rc = ten_nodejs_ten_env_peek_property_value(
      ten_env_bridge, ten_string_get_raw_str(&name), cb_tsfn, &err);
  if (!rc) {
    napi_value js_error = ten_nodejs_create_error(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    // The JS callback will not be called, so we need to clean up the tsfn.
    ten_nodejs_tsfn_release(cb_tsfn);
    ten_error_deinit(&err);
    return js_error;
  }

  ten_error_deinit(&err);
  ten_string_deinit(&name);

  return js_null(env);
}
