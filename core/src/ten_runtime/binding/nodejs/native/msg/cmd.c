//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "js_native_api.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_cmd_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_cmd_register_class(napi_env env,
                                                napi_callback_info info) {
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

static void ten_nodejs_cmd_finalize(napi_env env, void *data, void *hint) {
  ten_nodejs_cmd_t *cmd_bridge = data;
  TEN_ASSERT(cmd_bridge, "Should not happen.");

  napi_delete_reference(env, cmd_bridge->msg.bridge.js_instance_ref);

  ten_nodejs_cmd_destroy(cmd_bridge);
}

static napi_value ten_nodejs_cmd_create(napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, cmd_name
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_string_t cmd_name;
  TEN_STRING_INIT(cmd_name);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &cmd_name);
  if (!rc) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get cmd_name.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_t error;
  ten_error_init(&error);

  ten_shared_ptr_t *c_cmd =
      ten_cmd_create(ten_string_get_raw_str(&cmd_name), &error);
  TEN_ASSERT(c_cmd, "Failed to create cmd.");

  ten_string_deinit(&cmd_name);

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

napi_value ten_nodejs_cmd_wrap(napi_env env, ten_shared_ptr_t *cmd) {
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Should not happen.");

  ten_nodejs_cmd_t *cmd_bridge = TEN_MALLOC(sizeof(ten_nodejs_cmd_t));
  TEN_ASSERT(cmd_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&cmd_bridge->msg, cmd);

  napi_value js_msg_name = NULL;
  napi_value js_create_shell_only_flag = NULL;

  const char *msg_name = ten_msg_get_name(cmd);
  TEN_ASSERT(msg_name, "Should not happen.");

  napi_status status =
      napi_create_string_utf8(env, msg_name, NAPI_AUTO_LENGTH, &js_msg_name);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_msg_name != NULL,
                      "Failed to create JS string: %d", status);

  status = napi_get_boolean(env, true, &js_create_shell_only_flag);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_create_shell_only_flag != NULL,
                      "Failed to create JS boolean: %d", status);

  napi_value argv[] = {js_msg_name, js_create_shell_only_flag};

  napi_value js_cmd = ten_nodejs_create_new_js_object_and_wrap(
      env, js_cmd_constructor_ref, cmd_bridge, ten_nodejs_cmd_finalize,
      &cmd_bridge->msg.bridge.js_instance_ref, 2, argv);
  ASSERT_IF_NAPI_FAIL(js_cmd != NULL, "Failed to create JS Cmd object.");

  return js_cmd;
}

napi_value ten_nodejs_cmd_module_init(napi_env env, napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_create);

  return exports;
}
