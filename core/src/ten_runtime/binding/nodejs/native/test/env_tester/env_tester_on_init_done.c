//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"

static void ten_env_tester_proxy_notify_on_init_done(
    ten_env_tester_t *ten_env_tester, TEN_UNUSED void *user_data) {
  TEN_ASSERT(ten_env_tester, "Should not happen.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_on_init_done(ten_env_tester, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

napi_value ten_nodejs_ten_env_tester_on_init_done(napi_env env,
                                                  napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // ten_env_tester
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

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_tester_proxy_notify(
      ten_env_tester_bridge->c_ten_env_tester_proxy,
      ten_env_tester_proxy_notify_on_init_done, NULL, &err);
  if (!rc) {
    TEN_LOGD("TEN/JS failed to on_init_done");

    ten_string_t code_str;
    ten_string_init_formatted(&code_str, "%d", ten_error_code(&err));

    status = napi_throw_error(env, ten_string_get_raw_str(&code_str),
                              ten_error_message(&err));
    ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to throw JS exception: %d",
                        status);

    ten_string_deinit(&code_str);
  }

  ten_error_deinit(&err);

  return js_undefined(env);
}
