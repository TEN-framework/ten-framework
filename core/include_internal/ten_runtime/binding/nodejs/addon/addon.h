//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_ADDON_SIGNATURE 0xAB6B372F015D9CE9U

typedef struct ten_nodejs_addon_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_nodejs_bridge_t bridge;

  ten_string_t addon_name;
  ten_addon_t c_addon;  // The corresponding C addon.

  ten_addon_host_t *c_addon_host;

  // @{
  // The following functions represent the JavaScript functions corresponding to
  // the addon interface API.
  ten_nodejs_tsfn_t *js_on_init;
  ten_nodejs_tsfn_t *js_on_deinit;
  ten_nodejs_tsfn_t *js_on_create_instance;
  // @}
} ten_nodejs_addon_t;

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_addon_check_integrity(
    ten_nodejs_addon_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_invoke_addon_js_on_init(napi_env env,
                                                                napi_value fn,
                                                                void *context,
                                                                void *data);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_invoke_addon_js_on_deinit(napi_env env,
                                                                  napi_value fn,
                                                                  void *context,
                                                                  void *data);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_invoke_addon_js_on_create_instance(
    napi_env env, napi_value fn, void *context, void *data);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_addon_module_init(napi_env env, napi_value exports);
