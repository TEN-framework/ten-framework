//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"

#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "ten_runtime/binding/common.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/sanitizer/thread_check.h"

static napi_ref js_ten_env_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_ten_env_register_class(napi_env env,
                                                    napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 1;
  napi_value argv[argc];  // TenEnv
  if (!ten_nodejs_get_js_func_args(env, info, argv, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to register JS TenEnv class.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  napi_status status =
      napi_create_reference(env, argv[0], 1, &js_ten_env_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create JS reference to JS TenEnv constructor.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create JS reference to JS TenEnv constructor: %d",
               status);
  }

  return js_undefined(env);
}

static void ten_nodejs_ten_env_destroy(ten_nodejs_ten_env_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

static void ten_nodejs_ten_env_finalize(napi_env env, void *data, void *hint) {
  ten_nodejs_ten_env_t *ten_env_bridge = data;
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  TEN_LOGD("TEN JS rte object is finalized.");

  // According to NAPI doc:
  // ====
  // Caution: The optional returned reference (if obtained) should be deleted
  // via napi_delete_reference __ONLY__ in response to the finalize callback
  // invocation. If it is deleted before then, then the finalize callback may
  // never be invoked. Therefore, when obtaining a reference a finalize callback
  // is also required in order to enable correct disposal of the reference.
  napi_delete_reference(env, ten_env_bridge->bridge.js_instance_ref);

  ten_nodejs_ten_env_destroy(ten_env_bridge);
}

bool ten_nodejs_ten_env_check_integrity(ten_nodejs_ten_env_t *self,
                                        bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_NODEJS_TEN_ENV_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

ten_nodejs_get_property_call_ctx_t *ten_nodejs_get_property_call_ctx_create(
    ten_nodejs_tsfn_t *cb_tsfn, ten_value_t *value, ten_error_t *error) {
  TEN_ASSERT(cb_tsfn, "Invalid argument.");

  ten_nodejs_get_property_call_ctx_t *ctx =
      (ten_nodejs_get_property_call_ctx_t *)TEN_MALLOC(
          sizeof(ten_nodejs_get_property_call_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->cb_tsfn = cb_tsfn;
  ctx->value = value;
  ctx->error = error;

  return ctx;
}

void ten_nodejs_get_property_call_ctx_destroy(
    ten_nodejs_get_property_call_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->value) {
    ten_value_destroy(ctx->value);
  }

  if (ctx->error) {
    ten_error_destroy(ctx->error);
  }

  TEN_FREE(ctx);
}

ten_nodejs_set_property_call_ctx_t *ten_nodejs_set_property_call_ctx_create(
    ten_nodejs_tsfn_t *cb_tsfn, bool success, ten_error_t *error) {
  TEN_ASSERT(cb_tsfn, "Invalid argument.");

  ten_nodejs_set_property_call_ctx_t *ctx =
      (ten_nodejs_set_property_call_ctx_t *)TEN_MALLOC(
          sizeof(ten_nodejs_set_property_call_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->cb_tsfn = cb_tsfn;
  ctx->success = success;
  ctx->error = error;

  return ctx;
}

void ten_nodejs_set_property_call_ctx_destroy(
    ten_nodejs_set_property_call_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->error) {
    ten_error_destroy(ctx->error);
  }

  TEN_FREE(ctx);
}

napi_value ten_nodejs_ten_env_create_new_js_object_and_wrap(
    napi_env env, ten_env_t *ten_env,
    ten_nodejs_ten_env_t **out_ten_env_bridge) {
  TEN_ASSERT(env, "Should not happen.");

  // RTE_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, false),
             "Invalid use of ten_env %p.", ten_env);

  ten_nodejs_ten_env_t *ten_env_bridge =
      (ten_nodejs_ten_env_t *)TEN_MALLOC(sizeof(ten_nodejs_ten_env_t));
  TEN_ASSERT(ten_env_bridge, "Failed to allocate memory.");

  ten_signature_set(&ten_env_bridge->signature, TEN_NODEJS_TEN_ENV_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(
      &ten_env_bridge->thread_check);

  ten_env_bridge->c_ten_env = ten_env;
  ten_env_bridge->c_ten_env_proxy = NULL;

  ten_binding_handle_set_me_in_target_lang((ten_binding_handle_t *)ten_env,
                                           ten_env_bridge);

  napi_value instance = ten_nodejs_create_new_js_object_and_wrap(
      env, js_ten_env_constructor_ref, ten_env_bridge,
      ten_nodejs_ten_env_finalize, &ten_env_bridge->bridge.js_instance_ref, 0,
      NULL);
  if (!instance) {
    goto error;
  }

  goto done;

error:
  if (ten_env_bridge) {
    TEN_FREE(ten_env_bridge);
    ten_env_bridge = NULL;
  }

done:
  if (out_ten_env_bridge) {
    *out_ten_env_bridge = ten_env_bridge;
  }

  return instance;
}

napi_value ten_nodejs_ten_env_module_init(napi_env env, napi_value exports) {
  TEN_ASSERT(env && exports, "Should not happen.");

  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_register_class);

  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_configure_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_init_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_start_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_stop_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_deinit_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_on_create_instance_done);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_send_cmd);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_send_data);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_send_video_frame);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_send_audio_frame);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_return_result);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_is_property_exist);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_get_property_to_json);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_set_property_from_json);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_get_property_number);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_set_property_number);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_get_property_string);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_set_property_string);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_log_internal);
  EXPORT_FUNC(env, exports, ten_nodejs_ten_env_init_property_from_json);

  return exports;
}
