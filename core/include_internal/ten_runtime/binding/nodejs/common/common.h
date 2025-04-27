//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <node_api.h>

#include "ten_utils/lib/error.h"
#include "ten_utils/log/log.h"
#include "ten_utils/value/value.h"

typedef struct ten_smart_ptr_t ten_shared_ptr_t;

typedef struct ten_nodejs_bridge_t {
  // The following two fields are used to prevent the bridge instance from being
  // finalized. The bridge instance must only be destroyed after the
  // corresponding instances in both the C world and the JavaScript world have
  // been destroyed.
  ten_shared_ptr_t *sp_ref_by_c;
  ten_shared_ptr_t *sp_ref_by_js;

  // The reference to the JS instance.
  napi_ref js_instance_ref;
} ten_nodejs_bridge_t;

#define EXPORT_FUNC(env, exports, func)                \
  do {                                                 \
    ten_nodejs_export_func(env, exports, #func, func); \
  } while (0)

#define GOTO_LABEL_IF_NAPI_FAIL(label, expr, fmt, ...) \
  do {                                                 \
    if (!(expr)) {                                     \
      TEN_LOGE(fmt, ##__VA_ARGS__);                    \
      ten_nodejs_report_and_clear_error(env, status);  \
      goto label;                                      \
    }                                                  \
  } while (0)

#define ASSERT_IF_NAPI_FAIL(expr, fmt, ...)       \
  do {                                            \
    if (!(expr)) {                                \
      TEN_LOGE(fmt, ##__VA_ARGS__);               \
      TEN_ASSERT(0, "Should not happen.");        \
      /* NOLINTNEXTLINE(concurrency-mt-unsafe) */ \
      exit(EXIT_FAILURE);                         \
    }                                             \
  } while (0)

#define RETURN_UNDEFINED_IF_NAPI_FAIL(expr, fmt, ...) \
  do {                                                \
    if (!(expr)) {                                    \
      TEN_LOGE(fmt, ##__VA_ARGS__);                   \
      ten_nodejs_report_and_clear_error(env, status); \
      TEN_ASSERT(0, "Should not happen.");            \
      return js_undefined(env);                       \
    }                                                 \
  } while (0)

TEN_RUNTIME_PRIVATE_API napi_value js_undefined(napi_env env);

TEN_RUNTIME_PRIVATE_API napi_value js_null(napi_env env);

TEN_RUNTIME_PRIVATE_API napi_value
ten_nodejs_create_result_tuple(napi_env env, napi_value res, napi_value err);

TEN_RUNTIME_PRIVATE_API bool is_js_undefined(napi_env env, napi_value value);

TEN_RUNTIME_PRIVATE_API bool is_js_string(napi_env env, napi_value value);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_get_js_func_args(
    napi_env env, napi_callback_info info, napi_value *args, size_t argc);

TEN_RUNTIME_PRIVATE_API bool ten_nodejs_get_str_from_js(napi_env env,
                                                        napi_value val,
                                                        ten_string_t *str);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_report_and_clear_error_(
    napi_env env, napi_status status, const char *func, int line);

#define ten_nodejs_report_and_clear_error(env, status) \
  ten_nodejs_report_and_clear_error_(env, status, __FUNCTION__, __LINE__)

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_get_property(
    napi_env env, napi_value js_obj, const char *property_name);

TEN_RUNTIME_PRIVATE_API void ten_nodejs_export_func(napi_env env,
                                                    napi_value exports,
                                                    const char *func_name,
                                                    napi_callback func);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_create_new_js_object_and_wrap(
    napi_env env, napi_ref constructor_ref, void *bridge_obj,
    napi_finalize finalizer, napi_ref *bridge_weak_ref, size_t argc,
    const napi_value *argv);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_create_error(napi_env env,
                                                           ten_error_t *error);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_create_value_number(
    napi_env env, ten_value_t *value, ten_error_t *error);

TEN_RUNTIME_PRIVATE_API napi_value ten_nodejs_create_value_string(
    napi_env env, ten_value_t *value, ten_error_t *error);
