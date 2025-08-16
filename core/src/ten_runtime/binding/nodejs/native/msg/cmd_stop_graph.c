//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd.h"
#include "js_native_api.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_cmd_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_cmd_stop_graph_register_class(
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

static napi_value ten_nodejs_cmd_stop_graph_create(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_shared_ptr_t *c_cmd = ten_cmd_stop_graph_create();
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

napi_value ten_nodejs_cmd_stop_graph_module_init(napi_env env,
                                                 napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_stop_graph_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_cmd_stop_graph_create);

  return exports;
}
