//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/error/error.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/cmd.h"
#include "js_native_api.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_cmd_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_cmd_start_graph_register_class(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // Cmd constructor
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  napi_status status =
      napi_create_reference(env, args[0], 1, &js_cmd_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create JS reference to JS Cmd constructor.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create JS reference to JS Cmd constructor: %d",
               status);
  }

  return js_undefined(env);
}

static void ten_nodejs_cmd_destroy(ten_nodejs_cmd_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_nodejs_msg_deinit(&self->msg);

  TEN_FREE(self);
}

static void ten_nodejs_cmd_finalize(napi_env env, void *data,
                                    TEN_UNUSED void *hint) {
  ten_nodejs_cmd_t *cmd_bridge = data;
  TEN_ASSERT(cmd_bridge, "Should not happen.");

  napi_delete_reference(env, cmd_bridge->msg.bridge.js_instance_ref);

  ten_nodejs_cmd_destroy(cmd_bridge);
}

static napi_value ten_nodejs_cmd_start_graph_create(napi_env env,
                                                    napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_shared_ptr_t *c_cmd = ten_cmd_start_graph_create();
  TEN_ASSERT(c_cmd, "Failed to create cmd.");

  ten_nodejs_cmd_t *cmd_bridge = TEN_MALLOC(sizeof(ten_nodejs_cmd_t));
  TEN_ASSERT(cmd_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&cmd_bridge->msg, c_cmd);
  // Decrement the reference count of c_cmd to indicate that the JS cmd takes
  // the full ownership of this c_cmd, in other words, when the JS cmd is
  // finalized, its C cmd would be destroyed, too.
  ten_shared_ptr_destroy(c_cmd);

  napi_status status =
      napi_wrap(env, args[0], cmd_bridge, ten_nodejs_cmd_finalize, NULL,
                &cmd_bridge->msg.bridge.js_instance_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to wrap JS Cmd object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to wrap JS Cmd object: %d", status);
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_cmd_start_graph_set_predefined_graph_name(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, predefined_graph_name
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_cmd_t *cmd_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && cmd_bridge != NULL,
                                "Failed to get cmd bridge: %d", status);
  TEN_ASSERT(cmd_bridge, "Should not happen.");

  ten_string_t predefined_graph_name;
  TEN_STRING_INIT(predefined_graph_name);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &predefined_graph_name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get predefined graph name",
                                NULL);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool result = ten_cmd_start_graph_set_predefined_graph_name(
      cmd_bridge->msg.msg, ten_string_get_raw_str(&predefined_graph_name),
      &err);

  ten_string_deinit(&predefined_graph_name);

  napi_value js_error = NULL;
  if (!result) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_cmd_start_graph_set_graph_from_json_str(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, json_str
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_cmd_t *cmd_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && cmd_bridge != NULL,
                                "Failed to get cmd bridge: %d", status);
  TEN_ASSERT(cmd_bridge, "Should not happen.");

  ten_string_t json_str;
  TEN_STRING_INIT(json_str);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &json_str);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get JSON string", NULL);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool result = ten_cmd_start_graph_set_graph_from_json_str(
      cmd_bridge->msg.msg, ten_string_get_raw_str(&json_str), &err);

  ten_string_deinit(&json_str);

  napi_value js_error = NULL;
  if (!result) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

napi_value ten_nodejs_cmd_start_graph_module_init(napi_env env,
                                                  napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_start_graph_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_start_graph_create);
  EXPORT_FUNC(env, exports,
              ten_nodejs_cmd_start_graph_set_predefined_graph_name);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_start_graph_set_graph_from_json_str);

  return exports;
}
