//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"

static void ten_env_tester_proxy_notify_stop_test(
    ten_env_tester_t *ten_env_tester, void *user_data) {
  TEN_ASSERT(ten_env_tester, "Should not happen.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Should not happen.");

  ten_error_t *test_result = (ten_error_t *)user_data;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_stop_test(ten_env_tester, test_result, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  if (test_result) {
    ten_error_destroy(test_result);
  }
}

napi_value ten_nodejs_ten_env_tester_stop_test(napi_env env,
                                               napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 3;
  napi_value args[argc];  // ten_env_tester, error_code, error_message
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
      "Failed to get rte bridge: %d", status);
  TEN_ASSERT(ten_env_tester_bridge, "Should not happen.");
  TEN_ASSERT(
      ten_nodejs_ten_env_tester_check_integrity(ten_env_tester_bridge, true),
      "Should not happen.");

  ten_error_t *test_result = NULL;
  ten_error_code_t error_code = TEN_ERROR_CODE_OK;

  ten_string_t error_message;
  TEN_STRING_INIT(error_message);

  status = napi_get_value_int64(env, args[1], &error_code);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to get error code: %d", status);

  bool rc = ten_nodejs_get_str_from_js(env, args[2], &error_message);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get error message: %d", status);

  if (error_code != TEN_ERROR_CODE_OK) {
    test_result = ten_error_create();
    ten_error_set_error_code(test_result, error_code);
    ten_error_set_error_message(test_result,
                                ten_string_get_raw_str(&error_message));
  }

  ten_string_deinit(&error_message);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  rc = ten_env_tester_proxy_notify(
      ten_env_tester_bridge->c_ten_env_tester_proxy,
      ten_env_tester_proxy_notify_stop_test, test_result, &err);
  if (!rc) {
    TEN_LOGD("TEN/JS failed to stop_test");

    ten_string_t code_str;
    ten_string_init_formatted(&code_str, "%d", ten_error_code(&err));

    status = napi_throw_error(env, ten_string_get_raw_str(&code_str),
                              ten_error_message(&err));
    ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to throw JS exception: %d",
                        status);

    ten_string_deinit(&code_str);

    if (test_result) {
      ten_error_destroy(test_result);
    }
  }

  ten_error_deinit(&err);

  return js_undefined(env);
}
