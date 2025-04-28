//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/extension/extension.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"

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
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  ten_nodejs_extension_t *extension_bridge = NULL;
  status = napi_unwrap(env, args[1], (void **)&extension_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && extension_bridge != NULL,
                                "Failed to get extension bridge: %d", status);

  TEN_ASSERT(extension_bridge &&
                 ten_nodejs_extension_check_integrity(extension_bridge, true),
             "Should not happen.");

  void *context = NULL;
  status = napi_get_value_external(env, args[2], &context);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && context != NULL,
                                "Failed to get context: %d", status);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  TEN_ASSERT(ten_env_bridge->c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  bool rc = ten_env_on_create_instance_done(
      ten_env_bridge->c_ten_env, extension_bridge->c_extension, context, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  return js_undefined(env);
}
