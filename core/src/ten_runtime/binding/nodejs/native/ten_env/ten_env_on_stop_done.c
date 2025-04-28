//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"

static void ten_env_proxy_notify_on_stop_done(ten_env_t *ten_env,
                                              TEN_UNUSED void *user_data) {
  TEN_ASSERT(
      ten_env &&
          ten_env_check_integrity(
              ten_env,
              ten_env->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_stop_done(ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

napi_value ten_nodejs_ten_env_on_stop_done(napi_env env,
                                           napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // ten_env
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

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc =
      ten_env_proxy_notify_async(ten_env_bridge->c_ten_env_proxy,
                                 ten_env_proxy_notify_on_stop_done, NULL, &err);
  if (!rc) {
    TEN_LOGD("TEN/JS failed to on_stop_done.");

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
