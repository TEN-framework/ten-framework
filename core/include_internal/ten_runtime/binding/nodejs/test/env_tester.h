//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "include_internal/ten_runtime/test/env_tester_proxy.h"

#define TEN_NODEJS_TEN_ENV_TESTER_SIGNATURE 0x180B00AACEEF06E1U

typedef struct ten_nodejs_ten_env_tester_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  ten_env_tester_t *c_ten_env_tester;
  ten_env_tester_proxy_t *c_ten_env_tester_proxy;
} ten_nodejs_ten_env_tester_t;

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_module_init(napi_env env, napi_value exports);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_create_new_js_object_and_wrap(
    napi_env env, ten_env_tester_t *ten_env_tester,
    ten_nodejs_ten_env_tester_t **out_ten_env_tester_bridge);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_ten_env_tester_check_integrity(
    ten_nodejs_ten_env_tester_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_on_init_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_on_start_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_on_stop_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_on_deinit_done(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_send_cmd(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_send_data(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_ten_env_tester_send_video_frame(
    napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_ten_env_tester_send_audio_frame(
    napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_return_result(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_stop_test(napi_env env, napi_callback_info info);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_ten_env_tester_log_internal(napi_env env, napi_callback_info info);
