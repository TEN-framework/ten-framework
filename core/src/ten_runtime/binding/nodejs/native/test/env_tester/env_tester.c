//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"

#include "ten_runtime/binding/common.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_ten_env_tester_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_ten_env_tester_register_class(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 1;
  napi_value argv[argc];  // TenEnvTester
  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to register JS TenEnvTester class.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  napi_status status = napi_create_reference(
      env, argv[0], 1, &js_ten_env_tester_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(
        NULL, NAPI_AUTO_LENGTH,
        "Failed to create JS reference to JS TenEnvTester constructor.",
        NAPI_AUTO_LENGTH);
    TEN_ASSERT(
        0, "Failed to create JS reference to JS TenEnvTester constructor: %d",
        status);
  }

  return js_undefined(env);
}

bool ten_nodejs_ten_env_tester_check_integrity(
    ten_nodejs_ten_env_tester_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      TEN_NODEJS_TEN_ENV_TESTER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_nodejs_ten_env_tester_destroy(
    ten_nodejs_ten_env_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

static void ten_nodejs_ten_env_tester_finalize(napi_env env, void *data,
                                               TEN_UNUSED void *hint) {
  ten_nodejs_ten_env_tester_t *self = (ten_nodejs_ten_env_tester_t *)data;
  TEN_ASSERT(self && ten_nodejs_ten_env_tester_check_integrity(self, true),
             "Should not happen.");

  TEN_LOGD("TEN JS ten_env_tester object is finalized");

  napi_delete_reference(env, self->bridge.js_instance_ref);

  ten_nodejs_ten_env_tester_destroy(self);
}

napi_value ten_nodejs_ten_env_tester_create_new_js_object_and_wrap(
    napi_env env, ten_env_tester_t *ten_env_tester,
    ten_nodejs_ten_env_tester_t **out_ten_env_tester_bridge) {
  TEN_ASSERT(env, "Should not happen.");

  // RTE_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_tester, "Invalid use of ten_env_tester %p.",
             ten_env_tester);
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, false),
             "Invalid use of ten_env_tester %p.", ten_env_tester);

  ten_nodejs_ten_env_tester_t *env_tester_bridge =
      (ten_nodejs_ten_env_tester_t *)TEN_MALLOC(
          sizeof(ten_nodejs_ten_env_tester_t));
  TEN_ASSERT(env_tester_bridge,
             "Failed to allocate memory for env_tester_bridge.");

  ten_signature_set(&env_tester_bridge->signature,
                    TEN_NODEJS_TEN_ENV_TESTER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(
      &env_tester_bridge->thread_check);

  env_tester_bridge->c_ten_env_tester = ten_env_tester;
  env_tester_bridge->c_ten_env_tester_proxy = NULL;

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)ten_env_tester, env_tester_bridge);

  napi_value instance = ten_nodejs_create_new_js_object_and_wrap(
      env, js_ten_env_tester_constructor_ref, env_tester_bridge,
      ten_nodejs_ten_env_tester_finalize,
      &env_tester_bridge->bridge.js_instance_ref, 0, NULL);
  if (!instance) {
    goto error;
  }

  goto done;

error:
  if (env_tester_bridge) {
    TEN_FREE(env_tester_bridge);
    env_tester_bridge = NULL;
  }

done:
  if (out_ten_env_tester_bridge) {
    *out_ten_env_tester_bridge = env_tester_bridge;
  }

  return instance;
}

napi_value ten_nodejs_ten_env_tester_module_init(napi_env env,
                                                 napi_value exports) {
  TEN_ASSERT(env && exports, "Invalid arguments");

  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_register_class);

  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_on_init_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_on_start_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_on_stop_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_on_deinit_done);

  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_send_cmd);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_send_data);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_send_video_frame);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_send_audio_frame);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_return_result);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_stop_test);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_tester_log_internal);

  return exports;
}
