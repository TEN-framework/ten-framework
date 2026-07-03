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
#include "ten_runtime/app/app.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_APP_SIGNATURE 0x133066C5116F5EA4U

typedef struct ten_nodejs_app_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  ten_app_t *c_app;  // The corresponding C app.

  // @{
  // The following functions represent the JavaScript functions corresponding to
  // the app interface API.
  ten_nodejs_tsfn_t *js_on_configure;
  ten_nodejs_tsfn_t *js_on_init;
  ten_nodejs_tsfn_t *js_on_deinit;
  // @}
} ten_nodejs_app_t;

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_app_module_init(napi_env env, napi_value exports);
