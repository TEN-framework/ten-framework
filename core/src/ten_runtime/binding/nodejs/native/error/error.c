//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/error/error.h"

#include <string.h>

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"

static napi_ref js_error_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_error_register_class(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // Error constructor
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  napi_status status =
      napi_create_reference(env, args[0], 1, &js_error_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create JS reference to JS Error constructor.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create JS reference to JS Error constructor: %d",
               status);
  }

  return js_undefined(env);
}

napi_value ten_nodejs_error_wrap(napi_env env, ten_error_t *err) {
  TEN_ASSERT(err && ten_error_check_integrity(err), "Should not happen.");

  napi_value js_error_code = NULL;
  napi_value js_error_message = NULL;

  napi_status status = napi_create_int64(env, err->error_code, &js_error_code);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_error_code != NULL,
                      "Failed to create JS error code: %d", status);

  status =
      napi_create_string_utf8(env, ten_string_get_raw_str(&err->error_message),
                              NAPI_AUTO_LENGTH, &js_error_message);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_error_message != NULL,
                      "Failed to create JS error message: %d", status);

  napi_value argv[] = {js_error_code, js_error_message};

  napi_value js_error = NULL;

  // Get the JavaScript constructor function corresponding to the
  // 'constructor_ref'.
  napi_value js_constructor = NULL;
  status =
      napi_get_reference_value(env, js_error_constructor_ref, &js_constructor);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_constructor != NULL,
                      "Failed to get JS constructor: %d", status);

  status = napi_new_instance(env, js_constructor, 2, argv, &js_error);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_error != NULL,
                      "Failed to create JS error: %d", status);

  return js_error;
}

napi_value ten_nodejs_error_module_init(napi_env env, napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_error_register_class);

  return exports;
}