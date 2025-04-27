//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd_result.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "js_native_api.h"
#include "js_native_api_types.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_cmd_result_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_cmd_result_register_class(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // CmdResult constructor
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  napi_status status =
      napi_create_reference(env, args[0], 1, &js_cmd_result_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(
        NULL, NAPI_AUTO_LENGTH,
        "Failed to create JS reference to JS CmdResult constructor.",
        NAPI_AUTO_LENGTH);
    TEN_ASSERT(0,
               "Failed to create JS reference to JS CmdResult constructor: %d",
               status);
  }

  return js_undefined(env);
}

static void ten_nodejs_cmd_result_destroy(ten_nodejs_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_nodejs_msg_deinit(self);

  TEN_FREE(self);
}

static void ten_nodejs_cmd_result_finalize(napi_env env, void *data,
                                           void *hint) {
  ten_nodejs_cmd_result_t *cmd_result_bridge = data;
  TEN_ASSERT(cmd_result_bridge, "Should not happen.");

  napi_delete_reference(env, cmd_result_bridge->msg.bridge.js_instance_ref);

  ten_nodejs_cmd_result_destroy(&cmd_result_bridge->msg);
}

static napi_value ten_nodejs_cmd_result_create(napi_env env,
                                               napi_callback_info info) {
  const size_t argc = 3;
  napi_value args[argc];  // this, status_code, target_cmd
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  TEN_STATUS_CODE status_code = TEN_STATUS_CODE_INVALID;
  napi_status status =
      napi_get_value_uint32(env, args[1], (uint32_t *)&status_code);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get status_code.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_cmd_t *target_cmd = NULL;
  status = napi_unwrap(env, args[2], (void **)&target_cmd);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to unwrap target_cmd.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_t error;
  ten_error_init(&error);

  ten_shared_ptr_t *c_cmd_result =
      ten_cmd_result_create_from_cmd(status_code, target_cmd->msg.msg);
  TEN_ASSERT(c_cmd_result, "Failed to create cmd_result.");

  ten_nodejs_cmd_result_t *cmd_result_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_cmd_result_t));
  TEN_ASSERT(cmd_result_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&cmd_result_bridge->msg, c_cmd_result);
  // Decrement the reference count of c_cmd_result to indicate that the JS cmd
  // takes the full ownership of this c_cmd_result, in other words, when the JS
  // cmd is finalized, its C cmd would be destroyed, too.
  ten_shared_ptr_destroy(c_cmd_result);

  status =
      napi_wrap(env, args[0], cmd_result_bridge, ten_nodejs_cmd_result_finalize,
                NULL, &cmd_result_bridge->msg.bridge.js_instance_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to wrap JS CmdResult object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to wrap JS CmdResult object: %d", status);
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_cmd_result_get_status_code(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_cmd_result_t *cmd_result_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_result_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS CmdResult object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS CmdResult object: %d", status);
  }

  TEN_ASSERT(cmd_result_bridge, "Should not happen.");

  TEN_STATUS_CODE status_code =
      ten_cmd_result_get_status_code(cmd_result_bridge->msg.msg);

  napi_value js_status_code = NULL;
  status = napi_create_uint32(env, (uint32_t)status_code, &js_status_code);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create status_code.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create status_code: %d", status);
  }

  return js_status_code;
}

static napi_value ten_nodejs_cmd_result_set_final(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, is_final

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_cmd_result_t *cmd_result_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_result_bridge);

  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS CmdResult object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS CmdResult object: %d", status);
  }

  bool is_final = false;
  status = napi_get_value_bool(env, args[1], &is_final);

  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get is_final.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get is_final: %d", status);
  }

  if (!ten_cmd_result_set_final(cmd_result_bridge->msg.msg, is_final, NULL)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to set final.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to set final.");
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_cmd_result_is_final(napi_env env,
                                                 napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_cmd_result_t *cmd_result_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_result_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS CmdResult object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS CmdResult object: %d", status);
  }

  bool is_final = ten_cmd_result_is_final(cmd_result_bridge->msg.msg, NULL);

  napi_value js_is_final = NULL;
  status = napi_get_boolean(env, is_final, &js_is_final);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create is_final.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create is_final: %d", status);
  }

  return js_is_final;
}

static napi_value ten_nodejs_cmd_result_is_completed(napi_env env,
                                                     napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_cmd_result_t *cmd_result_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&cmd_result_bridge);

  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS CmdResult object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS CmdResult object: %d", status);
  }

  bool is_completed =
      ten_cmd_result_is_completed(cmd_result_bridge->msg.msg, NULL);

  napi_value js_is_completed = NULL;
  status = napi_get_boolean(env, is_completed, &js_is_completed);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create is_completed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create is_completed: %d", status);
  }

  return js_is_completed;
}

napi_value ten_nodejs_cmd_result_wrap(napi_env env,
                                      ten_shared_ptr_t *cmd_result) {
  TEN_ASSERT(cmd_result && ten_msg_check_integrity(cmd_result),
             "Should not happen.");

  ten_nodejs_cmd_result_t *cmd_result_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_cmd_result_t));
  TEN_ASSERT(cmd_result_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&cmd_result_bridge->msg, cmd_result);

  napi_value js_status_code = NULL;
  napi_value js_target_cmd = NULL;
  napi_value js_create_shell_only_flag = NULL;

  TEN_STATUS_CODE status_code = ten_cmd_result_get_status_code(cmd_result);
  napi_status status =
      napi_create_uint32(env, (uint32_t)status_code, &js_status_code);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create status_code.");

  status = napi_get_null(env, &js_target_cmd);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create target_cmd.");

  status = napi_get_boolean(env, true, &js_create_shell_only_flag);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create shell_only_flag.");

  napi_value argv[] = {js_status_code, js_target_cmd,
                       js_create_shell_only_flag};

  napi_value js_cmd_result = ten_nodejs_create_new_js_object_and_wrap(
      env, js_cmd_result_constructor_ref, cmd_result_bridge,
      ten_nodejs_cmd_result_finalize,
      &cmd_result_bridge->msg.bridge.js_instance_ref, 3, argv);
  ASSERT_IF_NAPI_FAIL(js_cmd_result != NULL, "Failed to create JS Cmd object.");

  return js_cmd_result;
}

napi_value ten_nodejs_cmd_result_module_init(napi_env env, napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_create);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_get_status_code);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_set_final);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_is_final);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_result_is_completed);

  return exports;
}
