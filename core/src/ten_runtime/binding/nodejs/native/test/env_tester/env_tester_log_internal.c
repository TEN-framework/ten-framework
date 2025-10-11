//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include/ten_runtime/common/error_code.h"
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/error/error.h"
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_env_tester_notify_log_ctx_t {
  int32_t level;
  ten_string_t func_name;
  ten_string_t file_name;
  int32_t line_no;
  ten_string_t category;
  ten_string_t msg;
} ten_env_tester_notify_log_ctx_t;

static ten_env_tester_notify_log_ctx_t *ten_env_tester_notify_log_ctx_create(
    void) {
  ten_env_tester_notify_log_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_tester_notify_log_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->level = 0;
  TEN_STRING_INIT(ctx->func_name);
  TEN_STRING_INIT(ctx->file_name);
  ctx->line_no = 0;
  TEN_STRING_INIT(ctx->category);
  TEN_STRING_INIT(ctx->msg);

  return ctx;
}

static void ten_env_tester_notify_log_ctx_destroy(
    ten_env_tester_notify_log_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_string_deinit(&ctx->func_name);
  ten_string_deinit(&ctx->file_name);
  ten_string_deinit(&ctx->category);
  ten_string_deinit(&ctx->msg);

  TEN_FREE(ctx);
}

static void ten_env_tester_proxy_notify_log(ten_env_tester_t *ten_env_tester,
                                            void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Should not happen.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Should not happen.");

  ten_env_tester_notify_log_ctx_t *ctx = user_data;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_env_tester_log(ten_env_tester, ctx->level,
                     ten_string_get_raw_str(&ctx->func_name),
                     ten_string_get_raw_str(&ctx->file_name), ctx->line_no,
                     ten_string_get_raw_str(&ctx->msg),
                     ten_string_get_raw_str(&ctx->category), NULL, NULL);

  ten_env_tester_notify_log_ctx_destroy(ctx);
}

napi_value ten_nodejs_ten_env_tester_log_internal(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 7;
  napi_value args[argc];  // ten_env_tester, level, func_name, file_name,
                          // line_no, category, msg
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge = NULL;
  napi_status status =
      napi_unwrap(env, args[0], (void **)&ten_env_tester_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && ten_env_tester_bridge != NULL,
      "Failed to get ten_env_tester bridge: %d", status);

  TEN_ASSERT(ten_env_tester_bridge, "Should not happen.");
  TEN_ASSERT(
      ten_nodejs_ten_env_tester_check_integrity(ten_env_tester_bridge, true),
      "Should not happen.");

  if (ten_env_tester_bridge->c_ten_env_tester_proxy == NULL) {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    ten_error_set(&err, TEN_ERROR_CODE_TEN_IS_CLOSED,
                  "ten_env.log_internal() failed because ten is closed.");

    napi_value js_error = ten_nodejs_error_wrap(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    ten_error_deinit(&err);

    return js_error;
  }

  ten_env_tester_notify_log_ctx_t *notify_info =
      ten_env_tester_notify_log_ctx_create();
  TEN_ASSERT(notify_info, "Failed to create log notify_info.");

  status = napi_get_value_int32(env, args[1], &notify_info->level);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to get log level: %d", status);

  bool rc = ten_nodejs_get_str_from_js(env, args[2], &notify_info->func_name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get function name.");

  rc = ten_nodejs_get_str_from_js(env, args[3], &notify_info->file_name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get file name.");

  status = napi_get_value_int32(env, args[4], &notify_info->line_no);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to get line number: %d", status);

  napi_value js_category = args[5];
  if (is_js_string(env, js_category)) {
    rc = ten_nodejs_get_str_from_js(env, args[5], &notify_info->category);
    RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get category.");
  }

  rc = ten_nodejs_get_str_from_js(env, args[6], &notify_info->msg);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get message.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  rc = ten_env_tester_proxy_notify(
      ten_env_tester_bridge->c_ten_env_tester_proxy,
      ten_env_tester_proxy_notify_log, notify_info, &err);
  if (!rc) {
    napi_value js_error = ten_nodejs_error_wrap(env, &err);
    RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

    ten_error_deinit(&err);
    ten_env_tester_notify_log_ctx_destroy(notify_info);

    return js_error;
  }

  ten_error_deinit(&err);

  return js_undefined(env);
}
