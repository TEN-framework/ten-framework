//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/test/extension_tester.h"

#include "include_internal/ten_runtime/binding/nodejs/error/error.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/data.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_nodejs_extension_tester_async_run_data_t {
  ten_nodejs_extension_tester_t *extension_tester_bridge;
  napi_deferred deferred;
  napi_async_work work;
  int async_action_status;
  ten_error_t *test_result;
} ten_nodejs_extension_tester_async_run_data_t;

typedef struct ten_nodejs_extension_tester_on_xxx_call_info_t {
  ten_nodejs_extension_tester_t *extension_tester_bridge;
  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge;
  ten_env_tester_t *ten_env_tester;
  ten_env_tester_proxy_t *ten_env_tester_proxy;
} ten_nodejs_extension_tester_on_xxx_call_info_t;

typedef struct ten_nodejs_extension_tester_on_msg_call_info_t {
  ten_nodejs_extension_tester_t *extension_tester_bridge;
  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge;
  ten_shared_ptr_t *msg;
} ten_nodejs_extension_tester_on_msg_call_info_t;

static bool ten_nodejs_extension_tester_check_integrity(
    ten_nodejs_extension_tester_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      TEN_NODEJS_EXTENSION_TESTER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_nodejs_extension_tester_detach_callbacks(
    ten_nodejs_extension_tester_t *self) {
  TEN_ASSERT(self && ten_nodejs_extension_tester_check_integrity(self, true),
             "Should not happen.");

  ten_nodejs_tsfn_dec_rc(self->js_on_init);
  ten_nodejs_tsfn_dec_rc(self->js_on_start);
  ten_nodejs_tsfn_dec_rc(self->js_on_stop);
  ten_nodejs_tsfn_dec_rc(self->js_on_deinit);
  ten_nodejs_tsfn_dec_rc(self->js_on_cmd);
  ten_nodejs_tsfn_dec_rc(self->js_on_data);
  ten_nodejs_tsfn_dec_rc(self->js_on_audio_frame);
  ten_nodejs_tsfn_dec_rc(self->js_on_video_frame);
}

static void ten_nodejs_extension_tester_destroy(
    ten_nodejs_extension_tester_t *self) {
  TEN_ASSERT(self && ten_nodejs_extension_tester_check_integrity(self, true),
             "Should not happen.");
  ten_nodejs_extension_tester_detach_callbacks(self);
  ten_sanitizer_thread_check_deinit(&self->thread_check);
  TEN_FREE(self);
}

static void ten_nodejs_extension_tester_finalize(napi_env env, void *data,
                                                 TEN_UNUSED void *hint) {
  TEN_ASSERT(env && data, "Should not happen.");

  TEN_LOGD("ten_nodejs_extension_tester_finalize()");

  ten_nodejs_extension_tester_t *extension_tester_bridge = data;
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge, true),
      "Should not happen.");

  napi_status status = napi_ok;

  status = napi_delete_reference(
      env, extension_tester_bridge->bridge.js_instance_ref);
  TEN_ASSERT(status == napi_ok,
             "Failed to delete JS extension tester & bridge: %d", status);

  extension_tester_bridge->bridge.js_instance_ref = NULL;
  // Destroy the underlying TEN C extension tester.
  ten_extension_tester_destroy(extension_tester_bridge->c_extension_tester);

  ten_nodejs_extension_tester_destroy(extension_tester_bridge);
}

static void proxy_on_init(ten_extension_tester_t *extension_tester,
                          ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge &&
          ten_nodejs_extension_tester_check_integrity(
              extension_tester_bridge,
              // TEN_NOLINTNEXTLINE(thread-check)
              // thread-check: The ownership of the extension_tester_bridge is
              // the JS main thread, therefore, in order to maintain thread
              // safety, we use semaphore below to prevent JS main thread and
              // the TEN C extension tester thread access the extension tester
              // bridge at the same time.
              false),
      "Should not happen.");

  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester = ten_env_tester;
  call_info->ten_env_tester_proxy =
      ten_env_tester_proxy_create(ten_env_tester, NULL);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_init, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_init()");
    TEN_FREE(call_info);

    // Failed to call JS on_init(), so that we need to call on_init_done()
    // here to let RTE runtime proceed.
    ten_env_tester_on_init_done(ten_env_tester, NULL);
  }
}

static void proxy_on_start(ten_extension_tester_t *extension_tester,
                           ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge &&
          ten_nodejs_extension_tester_check_integrity(
              extension_tester_bridge,
              // TEN_NOLINTNEXTLINE(thread-check)
              // thread-check: The ownership of the extension_tester_bridge is
              // the JS main thread, therefore, in order to maintain thread
              // safety, we use semaphore below to prevent JS main thread and
              // the TEN C extension tester thread access the extension tester
              // bridge at the same time.
              false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_start, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_start()");
    TEN_FREE(call_info);

    // Failed to call JS on_start(), so that we need to call on_start_done()
    // here to let RTE runtime proceed.
    ten_env_tester_on_start_done(ten_env_tester, NULL);
  }
}

static void proxy_on_stop(ten_extension_tester_t *extension_tester,
                          ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_stop, call_info);
  TEN_ASSERT(rc, "Failed to call extension tester on_stop()");
}

static void proxy_on_deinit(ten_extension_tester_t *extension_tester,
                            ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_deinit, call_info);
  TEN_ASSERT(rc, "Failed to call extension tester on_deinit()");
}

static void proxy_on_cmd(ten_extension_tester_t *extension_tester,
                         ten_env_tester_t *ten_env_tester,
                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;
  call_info->msg = ten_shared_ptr_clone(cmd);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_cmd, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_cmd()");
    TEN_FREE(call_info);
  }
}

static void proxy_on_data(ten_extension_tester_t *extension_tester,
                          ten_env_tester_t *ten_env_tester,
                          ten_shared_ptr_t *data) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;
  call_info->msg = ten_shared_ptr_clone(data);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_data, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_data()");
    TEN_FREE(call_info);
  }
}

static void proxy_on_audio_frame(ten_extension_tester_t *extension_tester,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;
  call_info->msg = ten_shared_ptr_clone(audio_frame);

  bool rc = ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_audio_frame,
                                   call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_audio_frame()");
    TEN_FREE(call_info);
  }
}

static void proxy_on_video_frame(ten_extension_tester_t *extension_tester,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(extension_tester, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(extension_tester, true),
             "Invalid argument.");
  TEN_ASSERT(ten_env_tester, "Invalid argument.");
  TEN_ASSERT(ten_env_tester_check_integrity(ten_env_tester, true),
             "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)extension_tester);
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge,
                                     // TEN_NOLINTNEXTLINE(thread-check)
                                     false),
      "Should not happen.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)ten_env_tester);
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge,
                                          // TEN_NOLINTNEXTLINE(thread-check)
                                          false),
             "Should not happen.");

  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_tester_bridge = extension_tester_bridge;
  call_info->ten_env_tester_bridge = ten_env_tester_bridge;
  call_info->msg = ten_shared_ptr_clone(video_frame);

  bool rc = ten_nodejs_tsfn_invoke(extension_tester_bridge->js_on_video_frame,
                                   call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension tester on_video_frame()");
    TEN_FREE(call_info);
  }
}

static void ten_nodejs_invoke_extension_tester_js_on_init(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_xxx_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  TEN_ASSERT(call_info->extension_tester_bridge &&
                 ten_nodejs_extension_tester_check_integrity(
                     call_info->extension_tester_bridge, true),
             "Invalid argument.");

  // Export the C ten_env_tester to the JS side.
  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge = NULL;
  napi_value js_ten_env_tester =
      ten_nodejs_ten_env_tester_create_new_js_object_and_wrap(
          env, call_info->ten_env_tester, &ten_env_tester_bridge);
  TEN_ASSERT(js_ten_env_tester, "Failed to create JS ten_env_tester object.");

  ten_env_tester_bridge->c_ten_env_tester_proxy =
      call_info->ten_env_tester_proxy;
  TEN_ASSERT(ten_env_tester_bridge->c_ten_env_tester_proxy,
             "Failed to set C ten_env_tester_proxy.");

  // Increase the reference count of the JS ten_env_tester object to prevent
  // it from being garbage collected.
  uint32_t js_ten_env_tester_ref_count = 0;
  napi_reference_ref(env, ten_env_tester_bridge->bridge.js_instance_ref,
                     &js_ten_env_tester_ref_count);

  napi_status status = napi_ok;

  {
    // Call the JS on_init() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester};
    status = napi_call_function(env, js_extension_tester, fn, 1, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_init()");
  }

  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_start(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_xxx_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  TEN_ASSERT(call_info->extension_tester_bridge &&
                 ten_nodejs_extension_tester_check_integrity(
                     call_info->extension_tester_bridge, true),
             "Invalid argument.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      call_info->ten_env_tester_bridge;
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge, true),
             "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_start() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, ten_env_tester_bridge->bridge.js_instance_ref, &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester};
    status = napi_call_function(env, js_extension_tester, fn, 1, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_start()");
  }

  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_stop(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_xxx_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      call_info->extension_tester_bridge;
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge, true),
      "Invalid argument.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      call_info->ten_env_tester_bridge;
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge, true),
             "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_stop() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, ten_env_tester_bridge->bridge.js_instance_ref, &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    // Call on_stop() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester};
    status = napi_call_function(env, js_extension_tester, fn, 1, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_stop()");
  }

  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_deinit(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_xxx_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_xxx_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      call_info->extension_tester_bridge;
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge, true),
      "Invalid argument.");

  ten_nodejs_ten_env_tester_t *ten_env_tester_bridge =
      call_info->ten_env_tester_bridge;
  TEN_ASSERT(ten_env_tester_bridge && ten_nodejs_ten_env_tester_check_integrity(
                                          ten_env_tester_bridge, true),
             "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_deinit() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, ten_env_tester_bridge->bridge.js_instance_ref, &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    // Call on_deinit() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester};
    status = napi_call_function(env, js_extension_tester, fn, 1, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_deinit()");
  }

  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_cmd(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_msg_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_cmd() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    // Get the TEN JS ten_env_tester object.
    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_tester_bridge->bridge.js_instance_ref,
        &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    napi_value js_cmd = ten_nodejs_cmd_wrap(env, call_info->msg);
    TEN_ASSERT(js_cmd != NULL, "Failed to wrap JS Cmd.");

    // Call on_cmd() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester, js_cmd};
    status = napi_call_function(env, js_extension_tester, fn, 2, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_cmd()");
  }

  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_data(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_msg_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_data() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    // Get the TEN JS ten_env_tester object.
    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_tester_bridge->bridge.js_instance_ref,
        &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    napi_value js_data = ten_nodejs_data_wrap(env, call_info->msg);
    TEN_ASSERT(js_data != NULL, "Failed to wrap JS Data.");

    // Call on_data() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester, js_data};
    status = napi_call_function(env, js_extension_tester, fn, 2, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_data()");
  }

  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_audio_frame(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_msg_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_audio_frame() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    // Get the TEN JS ten_env_tester object.
    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_tester_bridge->bridge.js_instance_ref,
        &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    napi_value js_audio_frame =
        ten_nodejs_audio_frame_wrap(env, call_info->msg);
    TEN_ASSERT(js_audio_frame != NULL, "Failed to wrap JS AudioFrame.");

    // Call on_audio_frame() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester, js_audio_frame};
    status = napi_call_function(env, js_extension_tester, fn, 2, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_audio_frame()");
  }

  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_tester_js_on_video_frame(
    napi_env env, napi_value fn, TEN_UNUSED void *context, void *data) {
  ten_nodejs_extension_tester_on_msg_call_info_t *call_info =
      (ten_nodejs_extension_tester_on_msg_call_info_t *)data;
  TEN_ASSERT(call_info, "Invalid argument.");

  napi_status status = napi_ok;

  {
    // Call the JS on_video_frame() function.

    // Get the TEN JS extension tester object.
    napi_value js_extension_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_tester_bridge->bridge.js_instance_ref,
        &js_extension_tester);
    TEN_ASSERT(status == napi_ok && js_extension_tester != NULL,
               "Failed to get JS extension tester reference.");

    // Get the TEN JS ten_env_tester object.
    napi_value js_ten_env_tester = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_tester_bridge->bridge.js_instance_ref,
        &js_ten_env_tester);
    TEN_ASSERT(status == napi_ok && js_ten_env_tester != NULL,
               "Failed to get JS ten_env_tester reference.");

    napi_value js_video_frame =
        ten_nodejs_video_frame_wrap(env, call_info->msg);
    TEN_ASSERT(js_video_frame != NULL, "Failed to wrap JS VideoFrame.");

    // Call on_video_frame() function.
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env_tester, js_video_frame};
    status = napi_call_function(env, js_extension_tester, fn, 2, argv, &result);
    TEN_ASSERT(status == napi_ok && result != NULL,
               "Failed to call JS extension tester on_video_frame()");
  }

  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_extension_tester_create_and_attach_callbacks(
    napi_env env, ten_nodejs_extension_tester_t *extension_tester_bridge) {
  TEN_ASSERT(env && extension_tester_bridge, "Should not happen.");
  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge, true),
      "Should not happen.");

  napi_value js_extension_tester = NULL;
  napi_status status = napi_get_reference_value(
      env, extension_tester_bridge->bridge.js_instance_ref,
      &js_extension_tester);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_extension_tester != NULL,
                      "Failed to get JS extension tester reference.");

  napi_value js_on_init_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onInitProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_init, env,
                    "[TSFN] extension_tester::onInit", js_on_init_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_init);

  napi_value js_on_start_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onStartProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_start, env,
                    "[TSFN] extension_tester::onStart", js_on_start_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_start);

  napi_value js_on_stop_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onStopProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_stop, env,
                    "[TSFN] extension_tester::onStop", js_on_stop_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_stop);

  napi_value js_on_deinit_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onDeinitProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_deinit, env,
                    "[TSFN] extension_tester::onDeinit", js_on_deinit_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_deinit);

  napi_value js_on_cmd_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onCmdProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_cmd, env,
                    "[TSFN] extension_tester::onCmd", js_on_cmd_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_cmd);

  napi_value js_on_data_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onDataProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_data, env,
                    "[TSFN] extension_tester::onData", js_on_data_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_data);

  napi_value js_on_audio_frame_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onAudioFrameProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_audio_frame, env,
                    "[TSFN] extension_tester::onAudioFrame",
                    js_on_audio_frame_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_audio_frame);

  napi_value js_on_video_frame_proxy =
      ten_nodejs_get_property(env, js_extension_tester, "onVideoFrameProxy");
  CREATE_JS_CB_TSFN(extension_tester_bridge->js_on_video_frame, env,
                    "[TSFN] extension_tester::onVideoFrame",
                    js_on_video_frame_proxy,
                    ten_nodejs_invoke_extension_tester_js_on_video_frame);
}

static napi_value ten_nodejs_extension_tester_create(napi_env env,
                                                     napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_nodejs_extension_tester_t *extension_tester_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_t));
  TEN_ASSERT(extension_tester_bridge, "Failed to allocate memory.");

  ten_signature_set(&extension_tester_bridge->signature,
                    TEN_NODEJS_EXTENSION_TESTER_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(
      &extension_tester_bridge->thread_check);

  // Wraps the native bridge instance ('extension_tester_bridge') in the
  // javascript ExtensionTester object (args[0]). And the returned reference
  // ('js_ref' here) is a weak reference, meaning it has a reference count of 0.
  napi_status status =
      napi_wrap(env, args[0], extension_tester_bridge,
                ten_nodejs_extension_tester_finalize, NULL,
                &extension_tester_bridge->bridge.js_instance_ref);
  GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                          "Failed to bind JS extension tester & bridge: %d",
                          status);

  // Create the underlying TEN C extension tester.
  extension_tester_bridge->c_extension_tester = ten_extension_tester_create(
      proxy_on_init, proxy_on_start, proxy_on_stop, proxy_on_deinit,
      proxy_on_cmd, proxy_on_data, proxy_on_audio_frame, proxy_on_video_frame);
  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)(extension_tester_bridge->c_extension_tester),
      extension_tester_bridge);

  goto done;

error:
  if (extension_tester_bridge) {
    TEN_FREE(extension_tester_bridge);
  }

done:
  return js_undefined(env);
}

static void ten_nodejs_extension_tester_async_run_execute(napi_env env,
                                                          void *data) {
  TEN_ASSERT(env, "Should not happen.");

  ten_nodejs_extension_tester_async_run_data_t *async_run_data =
      (ten_nodejs_extension_tester_async_run_data_t *)data;
  TEN_ASSERT(async_run_data, "Should not happen.");

  ten_error_t *test_result = ten_error_create();

  // Run the TEN extension tester.
  bool rc = ten_extension_tester_run(
      async_run_data->extension_tester_bridge->c_extension_tester, test_result);
  if (!rc) {
    async_run_data->test_result = test_result;
  } else {
    ten_error_destroy(test_result);
  }

  TEN_LOGI("ten_extension_tester_run run done");

  async_run_data->async_action_status = 0;
}

static void ten_nodejs_extension_tester_release_js_on_xxx_tsfn(
    ten_nodejs_extension_tester_t *extension_tester_bridge) {
  TEN_ASSERT(extension_tester_bridge, "Should not happen.");

  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_init);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_start);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_stop);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_deinit);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_cmd);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_data);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_audio_frame);
  ten_nodejs_tsfn_release(extension_tester_bridge->js_on_video_frame);
}

static void ten_nodejs_extension_tester_async_run_complete(
    napi_env env, TEN_UNUSED napi_status status, void *data) {
  TEN_ASSERT(env, "Should not happen.");

  ten_nodejs_extension_tester_async_run_data_t *async_run_data =
      (ten_nodejs_extension_tester_async_run_data_t *)data;
  TEN_ASSERT(async_run_data, "Should not happen.");

  int async_action_status = async_run_data->async_action_status;
  if (async_action_status == 0) {
    if (async_run_data->test_result) {
      napi_value js_error =
          ten_nodejs_error_wrap(env, async_run_data->test_result);
      napi_resolve_deferred(env, async_run_data->deferred, js_error);
      ten_error_destroy(async_run_data->test_result);
    } else {
      napi_resolve_deferred(env, async_run_data->deferred, js_undefined(env));
    }
  } else {
    napi_reject_deferred(env, async_run_data->deferred, js_undefined(env));
  }

  // From now on, the JS on_xxx callback(s) are useless, so release them all.
  ten_nodejs_extension_tester_release_js_on_xxx_tsfn(
      async_run_data->extension_tester_bridge);

  napi_delete_async_work(env, async_run_data->work);
  TEN_FREE(async_run_data);
}

static napi_value ten_nodejs_extension_tester_run(napi_env env,
                                                  napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  TEN_LOGD("ten_nodejs_extension_tester_run()");

  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_extension_tester_t *extension_tester_bridge = NULL;
  napi_status status =
      napi_unwrap(env, args[0], (void **)&extension_tester_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && extension_tester_bridge != NULL,
      "Failed to get extension tester bridge: %d", status);

  // Increase the reference count of the JS extension tester object to prevent
  // it from being garbage collected. The reference count will be decreased when
  // the extension tester is deinited.
  uint32_t js_extension_tester_ref_count = 0;
  status =
      napi_reference_ref(env, extension_tester_bridge->bridge.js_instance_ref,
                         &js_extension_tester_ref_count);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok, "Failed to reference JS extension tester: %d", status);

  // Create and attach callbacks which will be invoked during the runtime of the
  // TEN extension tester.
  // NOTE: The callbacks will be released when the extension tester run() is
  // done.
  ten_nodejs_extension_tester_create_and_attach_callbacks(
      env, extension_tester_bridge);

  ten_nodejs_extension_tester_async_run_data_t *async_run_data =
      TEN_MALLOC(sizeof(ten_nodejs_extension_tester_async_run_data_t));
  TEN_ASSERT(async_run_data, "Failed to allocate memory.");

  async_run_data->extension_tester_bridge = extension_tester_bridge;
  async_run_data->async_action_status = 1;
  async_run_data->test_result = NULL;

  napi_value promise = NULL;
  status = napi_create_promise(env, &async_run_data->deferred, &promise);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && promise != NULL,
                                "Failed to create promise: %d", status);

  status =
      napi_create_async_work(env, NULL, js_undefined(env),
                             ten_nodejs_extension_tester_async_run_execute,
                             ten_nodejs_extension_tester_async_run_complete,
                             async_run_data, &async_run_data->work);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && async_run_data->work != NULL,
      "Failed to create async work: %d", status);

  status = napi_queue_async_work(env, async_run_data->work);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to queue async work: %d", status);

  return promise;
}

static napi_value ten_nodejs_extension_tester_set_test_mode_single(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 3;
  napi_value args[argc];  // this, mode, property_json_str

  // If the function call fails, throw an exception directly, not expected to be
  // caught by developers
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  // Get the TEN extension tester object.
  ten_nodejs_extension_tester_t *extension_tester_bridge = NULL;
  napi_status status =
      napi_unwrap(env, args[0], (void **)&extension_tester_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && extension_tester_bridge != NULL,
      "Failed to get extension tester bridge: %d", status);

  ten_string_t mode;
  TEN_STRING_INIT(mode);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &mode);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get test mode", NULL);

  ten_string_t property_json_str;
  TEN_STRING_INIT(property_json_str);

  rc = ten_nodejs_get_str_from_js(env, args[2], &property_json_str);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property JSON string", NULL);

  ten_extension_tester_set_test_mode_single(
      extension_tester_bridge->c_extension_tester,
      ten_string_get_raw_str(&mode),
      ten_string_get_raw_str(&property_json_str));

  ten_string_deinit(&mode);
  ten_string_deinit(&property_json_str);

  return js_undefined(env);
}

static napi_value ten_nodejs_extension_tester_set_timeout(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env && info, "Should not happen.");

  const size_t argc = 2;
  napi_value args[argc];  // this, usec

  // If the function call fails, throw an exception directly, not expected to be
  // caught by developers
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  // Get the TEN extension tester object.
  ten_nodejs_extension_tester_t *extension_tester_bridge = NULL;
  napi_status status =
      napi_unwrap(env, args[0], (void **)&extension_tester_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && extension_tester_bridge != NULL,
      "Failed to get extension tester bridge: %d", status);

  int64_t usec = 0;
  status = napi_get_value_int64(env, args[1], &usec);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok, "Failed to get timeout: %d",
                                status);

  ten_extension_tester_set_timeout(extension_tester_bridge->c_extension_tester,
                                   usec);

  return js_undefined(env);
}

static napi_value ten_nodejs_extension_tester_on_end_of_life(
    napi_env env, napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  TEN_LOGD("ten_nodejs_extension_tester_on_end_of_life()");

  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_nodejs_extension_tester_t *extension_tester_bridge = NULL;
  napi_status status =
      napi_unwrap(env, args[0], (void **)&extension_tester_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(
      status == napi_ok && extension_tester_bridge != NULL,
      "Failed to get extension tester bridge: %d", status);

  TEN_ASSERT(
      extension_tester_bridge && ten_nodejs_extension_tester_check_integrity(
                                     extension_tester_bridge, true),
      "Should not happen.");

  // Decrease the reference count of the JS extension tester object.
  uint32_t js_extension_tester_ref_count = 0;
  status =
      napi_reference_unref(env, extension_tester_bridge->bridge.js_instance_ref,
                           &js_extension_tester_ref_count);
  TEN_ASSERT(status == napi_ok, "Failed to unreference JS extension tester: %d",
             status);

  // Log the reference count of the JS extension tester object.
  TEN_LOGD("JS extension tester reference count: %d",
           js_extension_tester_ref_count);

done:
  return js_undefined(env);
}

napi_value ten_nodejs_extension_tester_module_init(napi_env env,
                                                   napi_value exports) {
  TEN_ASSERT(env && exports, "Should not happen.");

  EXPORT_FUNC(env, exports, ten_nodejs_extension_tester_create);
  EXPORT_FUNC(env, exports, ten_nodejs_extension_tester_run);
  EXPORT_FUNC(env, exports, ten_nodejs_extension_tester_set_test_mode_single);
  EXPORT_FUNC(env, exports, ten_nodejs_extension_tester_set_timeout);

  EXPORT_FUNC(env, exports, ten_nodejs_extension_tester_on_end_of_life);
  return exports;
}