//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/extension/extension.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/data.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "js_native_api.h"
#include "js_native_api_types.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct extension_on_xxx_call_info_t {
  ten_nodejs_extension_t *extension_bridge;
  ten_nodejs_ten_env_t *ten_env_bridge;
  ten_env_t *ten_env;
  ten_env_proxy_t *ten_env_proxy;
} extension_on_xxx_call_info_t;

typedef struct extension_on_msg_call_info_t {
  ten_nodejs_extension_t *extension_bridge;
  ten_nodejs_ten_env_t *ten_env_bridge;
  ten_shared_ptr_t *msg;
} extension_on_msg_call_info_t;

bool ten_nodejs_extension_check_integrity(ten_nodejs_extension_t *self,
                                          bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_NODEJS_EXTENSION_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_nodejs_extension_detach_callbacks(
    ten_nodejs_extension_t *self) {
  TEN_ASSERT(self && ten_nodejs_extension_check_integrity(self, true),
             "Should not happen.");

  ten_nodejs_tsfn_dec_rc(self->js_on_configure);
  ten_nodejs_tsfn_dec_rc(self->js_on_init);
  ten_nodejs_tsfn_dec_rc(self->js_on_start);
  ten_nodejs_tsfn_dec_rc(self->js_on_stop);
  ten_nodejs_tsfn_dec_rc(self->js_on_deinit);
  ten_nodejs_tsfn_dec_rc(self->js_on_cmd);
  ten_nodejs_tsfn_dec_rc(self->js_on_data);
  ten_nodejs_tsfn_dec_rc(self->js_on_audio_frame);
  ten_nodejs_tsfn_dec_rc(self->js_on_video_frame);
}

static void ten_nodejs_extension_finalize(napi_env env, void *data,
                                          void *hint) {
  TEN_LOGI("TEN JS extension is finalized.");

  ten_nodejs_extension_t *extension_bridge = data;
  TEN_ASSERT(extension_bridge, "Should not happen.");

  napi_status status = napi_ok;
  status = napi_delete_reference(env, extension_bridge->bridge.js_instance_ref);
  TEN_ASSERT(status == napi_ok, "Failed to delete JS extension reference.");

  ten_nodejs_extension_detach_callbacks(extension_bridge);

  // Unreference the JS extension instance to allow it to be garbage collected.

  ten_extension_destroy(extension_bridge->c_extension);

  ten_sanitizer_thread_check_deinit(&extension_bridge->thread_check);

  TEN_FREE(extension_bridge);
}

static void ten_nodejs_invoke_extension_js_on_configure(napi_env env,
                                                        napi_value fn,
                                                        void *context,
                                                        void *data) {
  extension_on_xxx_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(
      call_info->extension_bridge && ten_nodejs_extension_check_integrity(
                                         call_info->extension_bridge, true),
      "Should not happen.");

  // Export the C ten_env object to the JS world.
  ten_nodejs_ten_env_t *ten_env_bridge = NULL;
  napi_value js_ten_env = ten_nodejs_ten_env_create_new_js_object_and_wrap(
      env, call_info->ten_env, &ten_env_bridge);
  TEN_ASSERT(js_ten_env, "Should not happen.");

  ten_env_bridge->c_ten_env_proxy = call_info->ten_env_proxy;
  TEN_ASSERT(ten_env_bridge->c_ten_env_proxy, "Should not happen.");

  // Increase the reference count of the JS ten_env object to prevent it from
  // being garbage collected.
  uint32_t js_ten_env_ref_count = 0;
  napi_reference_ref(env, ten_env_bridge->bridge.js_instance_ref,
                     &js_ten_env_ref_count);

  napi_status status = napi_ok;

  {
    // Call on_configure() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_extension, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_configure(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_configure().");

done:
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_init(napi_env env, napi_value fn,
                                                   void *context, void *data) {
  extension_on_xxx_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  ten_nodejs_extension_t *extension_bridge = call_info->extension_bridge;
  TEN_ASSERT(extension_bridge &&
                 ten_nodejs_extension_check_integrity(extension_bridge, true),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge = call_info->ten_env_bridge;
  TEN_ASSERT(ten_env_bridge &&
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_init() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, extension_bridge->bridge.js_instance_ref, &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    // Call on_init().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_extension, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_init(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_init().");

done:
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_start(napi_env env, napi_value fn,
                                                    void *context, void *data) {
  extension_on_xxx_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(
      call_info->extension_bridge && ten_nodejs_extension_check_integrity(
                                         call_info->extension_bridge, true),
      "Should not happen.");

  TEN_ASSERT(call_info->ten_env_bridge && ten_nodejs_ten_env_check_integrity(
                                              call_info->ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_start() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    // Call on_start().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_extension, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_start(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_start().");

done:
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_stop(napi_env env, napi_value fn,
                                                   void *context, void *data) {
  extension_on_xxx_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(
      call_info->extension_bridge && ten_nodejs_extension_check_integrity(
                                         call_info->extension_bridge, true),
      "Should not happen.");

  TEN_ASSERT(call_info->ten_env_bridge && ten_nodejs_ten_env_check_integrity(
                                              call_info->ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_stop() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    // Call on_stop().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_extension, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_stop(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_stop().");

done:
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_deinit(napi_env env,
                                                     napi_value fn,
                                                     void *context,
                                                     void *data) {
  extension_on_xxx_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  TEN_ASSERT(
      call_info->extension_bridge && ten_nodejs_extension_check_integrity(
                                         call_info->extension_bridge, true),
      "Should not happen.");

  TEN_ASSERT(call_info->ten_env_bridge && ten_nodejs_ten_env_check_integrity(
                                              call_info->ten_env_bridge, true),
             "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_deinit() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    // Call on_deinit().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env};
    status = napi_call_function(env, js_extension, fn, 1, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_deinit(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_deinit().");

done:
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_cmd(napi_env env, napi_value fn,
                                                  void *context, void *data) {
  extension_on_msg_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_cmd() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    // Get the TEN JS ten_env.
    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    napi_value js_cmd = ten_nodejs_cmd_wrap(env, call_info->msg);
    GOTO_LABEL_IF_NAPI_FAIL(error, js_cmd != NULL, "Failed to wrap JS Cmd.");

    // Call on_cmd().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env, js_cmd};
    status = napi_call_function(env, js_extension, fn, 2, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_cmd(): %d", status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_cmd().");

done:
  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_data(napi_env env, napi_value fn,
                                                   void *context, void *data) {
  extension_on_msg_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_data() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    // Get the TEN JS ten_env.
    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    napi_value js_data = ten_nodejs_data_wrap(env, call_info->msg);
    GOTO_LABEL_IF_NAPI_FAIL(error, js_data != NULL, "Failed to wrap JS Data.");

    // Call on_data().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env, js_data};
    status = napi_call_function(env, js_extension, fn, 2, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_data(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_data().");

done:
  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_audio_frame(napi_env env,
                                                          napi_value fn,
                                                          void *context,
                                                          void *data) {
  extension_on_msg_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_audio_frame() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    // Get the TEN JS ten_env.
    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    napi_value js_audio_frame =
        ten_nodejs_audio_frame_wrap(env, call_info->msg);
    GOTO_LABEL_IF_NAPI_FAIL(error, js_audio_frame != NULL,
                            "Failed to wrap JS Data.");

    // Call on_audio_frame().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env, js_audio_frame};
    status = napi_call_function(env, js_extension, fn, 2, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_audio_frame(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_audio_frame().");

done:
  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_invoke_extension_js_on_video_frame(napi_env env,
                                                          napi_value fn,
                                                          void *context,
                                                          void *data) {
  extension_on_msg_call_info_t *call_info = data;
  TEN_ASSERT(call_info, "Should not happen.");

  napi_status status = napi_ok;

  {
    // Call on_video_frame() of the TEN JS extension.

    // Get the TEN JS extension.
    napi_value js_extension = NULL;
    status = napi_get_reference_value(
        env, call_info->extension_bridge->bridge.js_instance_ref,
        &js_extension);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_extension != NULL,
                            "Failed to get JS extension: %d", status);

    // Get the TEN JS ten_env.
    napi_value js_ten_env = NULL;
    status = napi_get_reference_value(
        env, call_info->ten_env_bridge->bridge.js_instance_ref, &js_ten_env);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok && js_ten_env != NULL,
                            "Failed to get JS ten_env: %d", status);

    napi_value js_video_frame =
        ten_nodejs_video_frame_wrap(env, call_info->msg);
    GOTO_LABEL_IF_NAPI_FAIL(error, js_video_frame != NULL,
                            "Failed to wrap JS Data.");

    // Call on_video_frame().
    napi_value result = NULL;
    napi_value argv[] = {js_ten_env, js_video_frame};
    status = napi_call_function(env, js_extension, fn, 2, argv, &result);
    GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                            "Failed to call JS extension on_video_frame(): %d",
                            status);
  }

  goto done;

error:
  TEN_LOGE("Failed to call JS extension on_video_frame().");

done:
  ten_shared_ptr_destroy(call_info->msg);
  TEN_FREE(call_info);
}

static void ten_nodejs_extension_create_and_attach_callbacks(
    napi_env env, ten_nodejs_extension_t *extension_bridge) {
  TEN_ASSERT(env, "Should not happen.");
  TEN_ASSERT(extension_bridge &&
                 ten_nodejs_extension_check_integrity(extension_bridge, true),
             "Should not happen.");

  napi_value js_extension = NULL;
  napi_status status = napi_get_reference_value(
      env, extension_bridge->bridge.js_instance_ref, &js_extension);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_extension != NULL,
                      "Failed to get JS extension instance.");

  napi_value js_on_configure_proxy =
      ten_nodejs_get_property(env, js_extension, "onConfigureProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_configure, env,
                    "[TSFN] extension::onConfigure", js_on_configure_proxy,
                    ten_nodejs_invoke_extension_js_on_configure);

  napi_value js_on_init_proxy =
      ten_nodejs_get_property(env, js_extension, "onInitProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_init, env,
                    "[TSFN] extension::onInit", js_on_init_proxy,
                    ten_nodejs_invoke_extension_js_on_init);

  napi_value js_on_start_proxy =
      ten_nodejs_get_property(env, js_extension, "onStartProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_start, env,
                    "[TSFN] extension::onStart", js_on_start_proxy,
                    ten_nodejs_invoke_extension_js_on_start);

  napi_value js_on_stop_proxy =
      ten_nodejs_get_property(env, js_extension, "onStopProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_stop, env,
                    "[TSFN] extension::onStop", js_on_stop_proxy,
                    ten_nodejs_invoke_extension_js_on_stop);

  napi_value js_on_deinit_proxy =
      ten_nodejs_get_property(env, js_extension, "onDeinitProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_deinit, env,
                    "[TSFN] extension::onDeinit", js_on_deinit_proxy,
                    ten_nodejs_invoke_extension_js_on_deinit);

  napi_value js_on_cmd_proxy =
      ten_nodejs_get_property(env, js_extension, "onCmdProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_cmd, env, "[TSFN] extension::onCmd",
                    js_on_cmd_proxy, ten_nodejs_invoke_extension_js_on_cmd);

  napi_value js_on_data_proxy =
      ten_nodejs_get_property(env, js_extension, "onDataProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_data, env,
                    "[TSFN] extension::onData", js_on_data_proxy,
                    ten_nodejs_invoke_extension_js_on_data);

  napi_value js_on_audio_frame_proxy =
      ten_nodejs_get_property(env, js_extension, "onAudioFrameProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_audio_frame, env,
                    "[TSFN] extension::onAudioFrame", js_on_audio_frame_proxy,
                    ten_nodejs_invoke_extension_js_on_audio_frame);

  napi_value js_on_video_frame_proxy =
      ten_nodejs_get_property(env, js_extension, "onVideoFrameProxy");
  CREATE_JS_CB_TSFN(extension_bridge->js_on_video_frame, env,
                    "[TSFN] extension::onVideoFrame", js_on_video_frame_proxy,
                    ten_nodejs_invoke_extension_js_on_video_frame);
}

static void ten_nodejs_extension_release_js_on_xxx_tsfn(
    ten_nodejs_extension_t *self) {
  TEN_ASSERT(self && ten_nodejs_extension_check_integrity(self, true),
             "Should not happen.");

  ten_nodejs_tsfn_release(self->js_on_configure);
  ten_nodejs_tsfn_release(self->js_on_init);
  ten_nodejs_tsfn_release(self->js_on_start);
  ten_nodejs_tsfn_release(self->js_on_stop);
  ten_nodejs_tsfn_release(self->js_on_deinit);
  ten_nodejs_tsfn_release(self->js_on_cmd);
  ten_nodejs_tsfn_release(self->js_on_data);
  ten_nodejs_tsfn_release(self->js_on_audio_frame);
  ten_nodejs_tsfn_release(self->js_on_video_frame);
}

static void proxy_on_configure(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_LOGI("extension proxy_on_configure");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  extension_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = NULL;
  call_info->ten_env = ten_env;
  call_info->ten_env_proxy = ten_env_proxy_create(ten_env, 1, NULL);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_bridge->js_on_configure, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_configure().");
    TEN_FREE(call_info);

    // Failed to call JS on_configure(), so that we need to call
    // on_configure_done() here to let TEN runtime proceed.
    ten_env_on_configure_done(ten_env, NULL);
  }
}

static void proxy_on_init(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_LOGI("extension proxy_on_init");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_init, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_init().");
    TEN_FREE(call_info);

    // Failed to call JS on_init(), so that we need to call on_init_done() here
    // to let TEN runtime proceed.
    ten_env_on_init_done(ten_env, NULL);
  }
}

static void proxy_on_start(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_LOGI("extension proxy_on_start");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_start, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_start().");
    TEN_FREE(call_info);

    // Failed to call JS on_start(), so that we need to call on_start_done()
    // here to let TEN runtime proceed.
    ten_env_on_start_done(ten_env, NULL);
  }
}

static void proxy_on_stop(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_LOGI("extension proxy_on_stop");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_stop, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_stop().");
    TEN_FREE(call_info);

    // Failed to call JS on_stop(), so that we need to call on_stop_done() here
    // to let TEN runtime proceed.
    ten_env_on_stop_done(ten_env, NULL);
  }
}

static void proxy_on_deinit(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_LOGI("extension proxy_on_deinit");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_xxx_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_xxx_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_deinit, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_deinit().");
    TEN_FREE(call_info);

    // Failed to call JS on_deinit(), so that we need to call on_deinit_done()
    // here to let TEN runtime proceed.
    ten_env_on_deinit_done(ten_env, NULL);
  }
}

static void proxy_on_cmd(ten_extension_t *self, ten_env_t *ten_env,
                         ten_shared_ptr_t *cmd) {
  TEN_LOGI("extension proxy_on_cmd");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;
  call_info->msg = ten_shared_ptr_clone(cmd);

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_cmd, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_cmd().");
    TEN_FREE(call_info);
  }
}
static void proxy_on_data(ten_extension_t *self, ten_env_t *ten_env,
                          ten_shared_ptr_t *data) {
  TEN_LOGI("extension proxy_on_data");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;
  call_info->msg = ten_shared_ptr_clone(data);

  bool rc = ten_nodejs_tsfn_invoke(extension_bridge->js_on_data, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_data().");
    TEN_FREE(call_info);
  }
}
static void proxy_on_audio_frame(ten_extension_t *self, ten_env_t *ten_env,
                                 ten_shared_ptr_t *frame) {
  TEN_LOGI("extension proxy_on_audio_frame");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;
  call_info->msg = ten_shared_ptr_clone(frame);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_bridge->js_on_audio_frame, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_audio_frame().");
    TEN_FREE(call_info);
  }
}
static void proxy_on_video_frame(ten_extension_t *self, ten_env_t *ten_env,
                                 ten_shared_ptr_t *frame) {
  TEN_LOGI("extension proxy_on_video_frame");

  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_nodejs_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(extension_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_extension_check_integrity(extension_bridge, false),
             "Should not happen.");

  ten_nodejs_ten_env_t *ten_env_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)ten_env);
  TEN_ASSERT(ten_env_bridge &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_nodejs_ten_env_check_integrity(ten_env_bridge, false),
             "Should not happen.");

  extension_on_msg_call_info_t *call_info =
      TEN_MALLOC(sizeof(extension_on_msg_call_info_t));
  TEN_ASSERT(call_info, "Failed to allocate memory.");

  call_info->extension_bridge = extension_bridge;
  call_info->ten_env_bridge = ten_env_bridge;
  call_info->msg = ten_shared_ptr_clone(frame);

  bool rc =
      ten_nodejs_tsfn_invoke(extension_bridge->js_on_video_frame, call_info);
  if (!rc) {
    TEN_LOGE("Failed to call extension on_video_frame().");
    TEN_FREE(call_info);
  }
}

static napi_value ten_nodejs_extension_create(napi_env env,
                                              napi_callback_info info) {
  TEN_ASSERT(env, "Should not happen.");

  ten_string_t name;
  TEN_STRING_INIT(name);

  const size_t argc = 2;
  napi_value args[argc];  // this, name
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  if (!ten_nodejs_get_str_from_js(env, args[1], &name)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get name.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  ten_nodejs_extension_t *extension_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_extension_t));
  TEN_ASSERT(extension_bridge, "Failed to allocate memory.");

  ten_signature_set(&extension_bridge->signature,
                    TEN_NODEJS_EXTENSION_SIGNATURE);

  // The ownership of the TEN extension_bridge is the JS main thread.
  ten_sanitizer_thread_check_init_with_current_thread(
      &extension_bridge->thread_check);

  // Wraps the native bridge instance ('extension_bridge') in the javascript
  // extension object (args[0]). And the returned reference ('js_ref' here) is
  // a weak reference, meaning it has a reference count of 0.
  napi_status status =
      napi_wrap(env, args[0], extension_bridge, ten_nodejs_extension_finalize,
                NULL, &extension_bridge->bridge.js_instance_ref);
  GOTO_LABEL_IF_NAPI_FAIL(error, status == napi_ok,
                          "Failed to bind JS extension & bridge: %d", status);

  // Increase the reference count of the JS extension, so that it can survive
  // after the ends of this native function.
  uint32_t js_extension_ref_count = 0;
  status = napi_reference_ref(env, extension_bridge->bridge.js_instance_ref,
                              &js_extension_ref_count);
  GOTO_LABEL_IF_NAPI_FAIL(
      error, status == napi_ok,
      "Failed to increase the reference count of JS extension: "
      "%d",
      status);

  // Create the underlying TEN C extension.
  extension_bridge->c_extension = ten_extension_create(
      ten_string_get_raw_str(&name), proxy_on_configure, proxy_on_init,
      proxy_on_start, proxy_on_stop, proxy_on_deinit, proxy_on_cmd,
      proxy_on_data, proxy_on_audio_frame, proxy_on_video_frame, NULL);
  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)(extension_bridge->c_extension),
      extension_bridge);

  ten_nodejs_extension_create_and_attach_callbacks(env, extension_bridge);

  goto done;

error:
  if (extension_bridge) {
    TEN_FREE(extension_bridge);
  }

done:
  ten_string_deinit(&name);
  return js_undefined(env);
}

static napi_value ten_nodejs_extension_on_end_of_life(napi_env env,
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

  ten_nodejs_extension_t *extension_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&extension_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && extension_bridge != NULL,
                                "Failed to get extension bridge: %d", status);

  TEN_ASSERT(extension_bridge &&
                 ten_nodejs_extension_check_integrity(extension_bridge, true),
             "Should not happen.");

  // From now on, the JS on_xxx callback(s) are useless, so release them all.
  ten_nodejs_extension_release_js_on_xxx_tsfn(extension_bridge);

  // Decrease the reference count of the JS extension object.
  uint32_t js_extension_ref_count = 0;
  status = napi_reference_unref(env, extension_bridge->bridge.js_instance_ref,
                                &js_extension_ref_count);
  TEN_ASSERT(status == napi_ok, "Failed to decrease the reference count.");

done:
  return js_undefined(env);
}

napi_value ten_nodejs_extension_module_init(napi_env env, napi_value exports) {
  TEN_ASSERT(env && exports, "Should not happen.");
  EXPORT_FUNC(env, exports, ten_nodejs_extension_create);
  EXPORT_FUNC(env, exports, ten_nodejs_extension_on_end_of_life);

  return exports;
}
