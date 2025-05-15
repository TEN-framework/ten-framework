//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"

static void ten_env_proxy_notify_on_deinit_done(ten_env_t *ten_env,
                                                void *user_data) {
  TEN_ASSERT(
      ten_env &&
          ten_env_check_integrity(
              ten_env,
              ten_env->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_proxy_t *ten_env_proxy = user_data;

  if (ten_env_proxy) {
    TEN_ASSERT(ten_env_proxy_get_thread_cnt(ten_env_proxy, NULL) == 1,
               "Should not happen.");

    bool rc = ten_env_proxy_release(ten_env_proxy, &err);
    TEN_ASSERT(rc, "Should not happen.");
  }

  bool rc = ten_env_on_deinit_done(ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

static void ten_app_addon_host_on_deinit_done(void *from, void *args) {
  ten_app_t *app = (ten_app_t *)from;
  TEN_ASSERT(app && ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_host_t *addon_host = (ten_addon_host_t *)args;
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_deinit_done(addon_host->ten_env, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

napi_value ten_nodejs_ten_env_on_deinit_done(napi_env env,
                                             napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // ten_env
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_ten_env_t *ten_env_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&ten_env_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && ten_env_bridge != NULL,
                                "Failed to get rte bridge: %d", status);
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  bool rc = false;

  if (ten_env_bridge->c_ten_env_proxy) {
    rc = ten_env_proxy_notify_async(ten_env_bridge->c_ten_env_proxy,
                                    ten_env_proxy_notify_on_deinit_done,
                                    ten_env_bridge->c_ten_env_proxy, &err);
  } else {
    TEN_ASSERT(ten_env_bridge->c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
               "Should not happen.");

    ten_env_t *ten_env = ten_env_bridge->c_ten_env;

    // Switch to the addon_host thread to call ten_env_on_init_done.
    ten_addon_host_t *addon_host = ten_env_get_attached_addon(ten_env);
    TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host, false),
               "Should not happen.");

    ten_app_t *app = addon_host->attached_app;
    TEN_ASSERT(app && ten_app_check_integrity(app, false),
               "Should not happen.");

    int post_task_rc = ten_runloop_post_task_tail(
        ten_app_get_attached_runloop(app), ten_app_addon_host_on_deinit_done,
        app, addon_host);
    rc = post_task_rc == 0;
  }

  // Reset the c_ten_env_proxy and c_ten_env to NULL.
  ten_env_bridge->c_ten_env_proxy = NULL;
  ten_env_bridge->c_ten_env = NULL;

  if (!rc) {
    TEN_LOGD("TEN/JS failed to on_deinit_done");

    ten_string_t code_str;
    ten_string_init_formatted(&code_str, "%d", ten_error_code(&err));

    status = napi_throw_error(env, ten_string_get_raw_str(&code_str),
                              ten_error_message(&err));
    ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to throw JS exception: %d",
                        status);

    ten_string_deinit(&code_str);
  }

  // Release the reference to the JS ten_env object.
  uint32_t js_ten_env_ref_count = 0;
  status = napi_reference_unref(env, ten_env_bridge->bridge.js_instance_ref,
                                &js_ten_env_ref_count);

  ten_error_deinit(&err);

  return js_undefined(env);
}
