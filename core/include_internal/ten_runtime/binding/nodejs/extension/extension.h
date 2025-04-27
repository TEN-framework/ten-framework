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
#include "include_internal/ten_runtime/extension/extension.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_EXTENSION_SIGNATURE 0xB7288D534BC053C3U

typedef struct ten_nodejs_extension_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  ten_extension_t *c_extension;  // The corresponding C extension.

  // @{
  // The following functions represent the JavaScript functions corresponding to
  // the extension interface API.
  ten_nodejs_tsfn_t *js_on_configure;
  ten_nodejs_tsfn_t *js_on_init;
  ten_nodejs_tsfn_t *js_on_start;
  ten_nodejs_tsfn_t *js_on_stop;
  ten_nodejs_tsfn_t *js_on_deinit;

  ten_nodejs_tsfn_t *js_on_cmd;
  ten_nodejs_tsfn_t *js_on_data;
  ten_nodejs_tsfn_t *js_on_audio_frame;
  ten_nodejs_tsfn_t *js_on_video_frame;
  // @}
} ten_nodejs_extension_t;

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_extension_check_integrity(
    ten_nodejs_extension_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_extension_module_init(napi_env env, napi_value exports);
