//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"
#include "ten_runtime/test/extension_tester.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_EXTENSION_TESTER_SIGNATURE 0x8F7D3E2A1B9C4D5EU

typedef struct ten_nodejs_extension_tester_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  ten_extension_tester_t *c_extension_tester;

  // @{
  // The following functions represent the JavaScript functions corresponding to
  // the app interface API.
  ten_nodejs_tsfn_t *js_on_init;
  ten_nodejs_tsfn_t *js_on_start;
  ten_nodejs_tsfn_t *js_on_stop;
  ten_nodejs_tsfn_t *js_on_deinit;
  ten_nodejs_tsfn_t *js_on_cmd;
  ten_nodejs_tsfn_t *js_on_data;
  ten_nodejs_tsfn_t *js_on_audio_frame;
  ten_nodejs_tsfn_t *js_on_video_frame;
  // @}
} ten_nodejs_extension_tester_t;

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_extension_tester_module_init(napi_env env, napi_value exports);
