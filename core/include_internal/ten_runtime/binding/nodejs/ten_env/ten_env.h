//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_TEN_ENV_SIGNATURE 0x180B00AACEEF06E0U

typedef struct ten_nodejs_ten_env_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  // @{
  // Above the binding layer, `c_ten_env_proxy` is used to interact with the C
  // runtime. However, since addon does not have the concept of a main thread,
  // it does not have the concept of `c_ten_env_proxy`. Therefore, only the
  // addon path uses `c_ten_env`, while all other TEN types use
  // `c_ten_env_proxy` for the associated `ten_env` concept.
  ten_env_t *c_ten_env;
  ten_env_proxy_t *c_ten_env_proxy;
  // @}
} ten_nodejs_ten_env_t;

typedef struct ten_nodejs_get_property_call_ctx_t {
  ten_nodejs_tsfn_t *cb_tsfn;
  ten_value_t *value;
  ten_error_t *error;
} ten_nodejs_get_property_call_ctx_t;

typedef struct ten_nodejs_set_property_call_ctx_t {
  ten_nodejs_tsfn_t *cb_tsfn;
  bool success;
  ten_error_t *error;
} ten_nodejs_set_property_call_ctx_t;

TEN_RUNTIME_PRIVATE_API ten_nodejs_get_property_call_ctx_t *
ten_nodejs_get_property_call_ctx_create(ten_nodejs_tsfn_t *cb_tsfn,
                                        ten_value_t *value, ten_error_t *error);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_get_property_call_ctx_destroy(
    ten_nodejs_get_property_call_ctx_t *ctx);

TEN_RUNTIME_PRIVATE_API ten_nodejs_set_property_call_ctx_t *
ten_nodejs_set_property_call_ctx_create(ten_nodejs_tsfn_t *cb_tsfn,
                                        bool success, ten_error_t *error);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_set_property_call_ctx_destroy(
    ten_nodejs_set_property_call_ctx_t *ctx);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_module_init(napi_env env, napi_value exports);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_create_new_js_object_and_wrap(
    napi_env env, ten_env_t *ten_env,
    ten_nodejs_ten_env_t **out_ten_env_bridge);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_ten_env_check_integrity(
    ten_nodejs_ten_env_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_on_configure_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_on_init_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_on_start_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_on_stop_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_on_deinit_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_ten_env_on_create_instance_done(
    napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_send_cmd(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_send_data(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_send_video_frame(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_send_audio_frame(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_return_result(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_ten_env_peek_property_value(
    ten_nodejs_ten_env_t *self, const char *path, ten_nodejs_tsfn_t *cb_tsfn,
    ten_error_t *error);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_ten_env_set_property_value(
    ten_nodejs_ten_env_t *self, const char *path, ten_value_t *value,
    ten_nodejs_tsfn_t *cb_tsfn, ten_error_t *error);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_is_property_exist(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_get_property_to_json(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_ten_env_set_property_from_json(
    napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_get_property_number(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_set_property_number(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_get_property_string(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_set_property_string(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_log_internal(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_ten_env_init_property_from_json(
    napi_env env, napi_callback_info info);
