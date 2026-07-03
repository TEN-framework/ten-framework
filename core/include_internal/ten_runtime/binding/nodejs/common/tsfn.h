//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_NODEJS_THREADSAFE_FUNCTION_SIGNATURE 0x1D11D6EF2722D8FBU

#define CREATE_JS_CB_TSFN(ten_tsfn, env, log_name, js_cb, tsfn_proxy_func)   \
  do {                                                                       \
    ten_tsfn =                                                               \
        ten_nodejs_tsfn_create((env), log_name, (js_cb), (tsfn_proxy_func)); \
    TEN_ASSERT((ten_tsfn), "Should not happen.");                            \
    ten_nodejs_tsfn_inc_rc((ten_tsfn));                                      \
  } while (0)

typedef struct ten_nodejs_tsfn_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_ref_t ref;  // Used to determine the timing of destroying this TSFN.

  ten_mutex_t *lock;
  ten_string_t name;

  napi_threadsafe_function tsfn;  // The TSFN itself.

  // The JS function which this tsfn calling.
  //
  // Because the JS functions pointed to by thread-safe functions may not
  // necessarily be normal functions existing in the JS world, they could be
  // dynamically created JS functions. The life cycle of dynamically generated
  // JS functions is bound to the thread-safe functions. Therefore, for unified
  // handling, TEN first acquires a reference to the JS function to prevent it
  // from being garbage collected. Then, when the thread-safe function is
  // finalized, TEN releases that reference, allowing the JS function to be
  // garbage collected.
  napi_ref js_func_ref;
} ten_nodejs_tsfn_t;

TEN_RUNTIME_API bool ten_nodejs_tsfn_check_integrity(ten_nodejs_tsfn_t *self,
                                                     bool check_thread);

TEN_RUNTIME_API ten_nodejs_tsfn_t *ten_nodejs_tsfn_create(
    napi_env env, const char *name, napi_value js_func,
    napi_threadsafe_function_call_js tsfn_proxy_func);

TEN_RUNTIME_API void ten_nodejs_tsfn_inc_rc(ten_nodejs_tsfn_t *self);

TEN_RUNTIME_API void ten_nodejs_tsfn_dec_rc(ten_nodejs_tsfn_t *self);

TEN_RUNTIME_API bool ten_nodejs_tsfn_invoke(ten_nodejs_tsfn_t *rte_tsfn,
                                            void *data);

TEN_RUNTIME_API void ten_nodejs_tsfn_release(ten_nodejs_tsfn_t *self);
