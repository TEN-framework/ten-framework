//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/go/interface/ten_runtime/extension.h"

#include <stdint.h>

#include "include_internal/ten_runtime/binding/go/extension/extension.h"
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

extern void tenGoExtensionOnConfigure(ten_go_handle_t go_extension,
                                      ten_go_handle_t go_ten_env);

extern void tenGoExtensionOnInit(ten_go_handle_t go_extension,
                                 ten_go_handle_t go_ten_env);

extern void tenGoExtensionOnStart(ten_go_handle_t go_extension,
                                  ten_go_handle_t go_ten_env);

extern void tenGoExtensionOnStop(ten_go_handle_t go_extension,
                                 ten_go_handle_t go_ten_env);

extern void tenGoExtensionOnDeinit(ten_go_handle_t go_extension,
                                   ten_go_handle_t go_ten_env);

extern void tenGoExtensionOnCmd(ten_go_handle_t go_extension,
                                ten_go_handle_t go_ten_env,
                                uintptr_t cmd_bridge_addr);

extern void tenGoExtensionOnData(ten_go_handle_t go_extension,
                                 ten_go_handle_t go_ten_env,
                                 uintptr_t data_bridge_addr);

extern void tenGoExtensionOnVideoFrame(ten_go_handle_t go_extension,
                                       ten_go_handle_t go_ten_env,
                                       uintptr_t video_frame_bridge_addr);

extern void tenGoExtensionOnAudioFrame(ten_go_handle_t go_extension,
                                       ten_go_handle_t go_ten_env,
                                       uintptr_t audio_frame_bridge_addr);

bool ten_go_extension_check_integrity(ten_go_extension_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_GO_EXTENSION_SIGNATURE) {
    return false;
  }

  return true;
}

ten_go_extension_t *ten_go_extension_reinterpret(uintptr_t bridge_addr) {
  TEN_ASSERT(bridge_addr, "Invalid argument.");

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  ten_go_extension_t *self = (ten_go_extension_t *)bridge_addr;
  TEN_ASSERT(ten_go_extension_check_integrity(self), "Invalid argument.");

  return self;
}

ten_go_handle_t ten_go_extension_go_handle(ten_go_extension_t *self) {
  TEN_ASSERT(ten_go_extension_check_integrity(self), "Should not happen.");

  return self->bridge.go_instance;
}

static void ten_go_extension_bridge_destroy(ten_go_extension_t *self) {
  TEN_ASSERT(ten_go_extension_check_integrity(self), "Should not happen.");

  ten_extension_t *c_extension = self->c_extension;
  TEN_ASSERT(c_extension, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In TEN world, the destroy operation should be performed in
  // any threads.
  TEN_ASSERT(ten_extension_check_integrity(c_extension, false),
             "Invalid use of extension %p.", c_extension);

  ten_extension_destroy(c_extension);
  TEN_FREE(self);
}

static void proxy_on_configure(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  ten_env_bridge->c_ten_env_proxy = ten_env_proxy_create(ten_env, 1, NULL);

  tenGoExtensionOnConfigure(ten_go_extension_go_handle(extension_bridge),
                            ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_init(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoExtensionOnInit(ten_go_extension_go_handle(extension_bridge),
                       ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_start(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoExtensionOnStart(ten_go_extension_go_handle(extension_bridge),
                        ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_stop(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoExtensionOnStop(ten_go_extension_go_handle(extension_bridge),
                       ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_deinit(ten_extension_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoExtensionOnDeinit(ten_go_extension_go_handle(extension_bridge),
                         ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_cmd(ten_extension_t *self, ten_env_t *ten_env,
                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_check_integrity(cmd), "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  // We only create the bridge instance in C world, and do _NOT_ call GO
  // function to create a GO cmd instance here. As the GO cmd instance is only
  // used by the GO extension, it can be created in GO world.
  ten_go_msg_t *msg_bridge = ten_go_msg_create(cmd);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionOnCmd(ten_go_extension_go_handle(extension_bridge),
                      ten_go_ten_env_go_handle(ten_env_bridge),
                      msg_bridge_addr);
}

static void proxy_on_data(ten_extension_t *self, ten_env_t *ten_env,
                          ten_shared_ptr_t *data) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  ten_go_msg_t *msg_bridge = ten_go_msg_create(data);

  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;
  tenGoExtensionOnData(ten_go_extension_go_handle(extension_bridge),
                       ten_go_ten_env_go_handle(ten_env_bridge),
                       msg_bridge_addr);
}

static void proxy_on_video_frame(ten_extension_t *self, ten_env_t *ten_env,
                                 ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  // Same as `on_cmd()`, the C world only care about the C bridge instance, but
  // does not need to create the GO instance from C. The GO instance can be
  // created in the GO world once the `tenGoExtensionOnVideoFrame()` is called
  // in GO world.
  ten_go_msg_t *msg_bridge = ten_go_msg_create(video_frame);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionOnVideoFrame(ten_go_extension_go_handle(extension_bridge),
                             ten_go_ten_env_go_handle(ten_env_bridge),
                             msg_bridge_addr);
}

static void proxy_on_audio_frame(ten_extension_t *self, ten_env_t *ten_env,
                                 ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_extension_get_ten_env(self) == ten_env, "Should not happen.");

  ten_go_extension_t *extension_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  ten_go_msg_t *msg_bridge = ten_go_msg_create(audio_frame);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionOnAudioFrame(ten_go_extension_go_handle(extension_bridge),
                             ten_go_ten_env_go_handle(ten_env_bridge),
                             msg_bridge_addr);
}

static ten_go_extension_t *ten_go_extension_create_internal(
    ten_go_handle_t go_extension, const char *name) {
  TEN_ASSERT(name, "Invalid argument.");

  ten_go_extension_t *extension_bridge =
      (ten_go_extension_t *)TEN_MALLOC(sizeof(ten_go_extension_t));
  TEN_ASSERT(extension_bridge, "Failed to allocate memory.");

  ten_signature_set(&extension_bridge->signature, TEN_GO_EXTENSION_SIGNATURE);
  extension_bridge->bridge.go_instance = go_extension;

  // The extension bridge instance is created and managed only by Go. When the
  // Go extension is finalized, the extension bridge instance will be destroyed.
  // Therefore, the C part should not hold any reference to the extension bridge
  // instance.
  extension_bridge->bridge.sp_ref_by_go =
      ten_shared_ptr_create(extension_bridge, ten_go_extension_bridge_destroy);
  extension_bridge->bridge.sp_ref_by_c = NULL;

  extension_bridge->c_extension = ten_extension_create(
      name, proxy_on_configure, proxy_on_init, proxy_on_start, proxy_on_stop,
      proxy_on_deinit, proxy_on_cmd, proxy_on_data, proxy_on_audio_frame,
      proxy_on_video_frame, NULL);

  ten_binding_handle_set_me_in_target_lang(
      &extension_bridge->c_extension->binding_handle, extension_bridge);

  return extension_bridge;
}

ten_go_error_t ten_go_extension_create(ten_go_handle_t go_extension,
                                       const void *name, int name_len,
                                       uintptr_t *bridge_addr) {
  TEN_ASSERT(go_extension > 0 && name && name_len > 0 && bridge_addr,
             "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_string_t extension_name;
  ten_string_init_formatted(&extension_name, "%.*s", name_len, name);

  ten_go_extension_t *extension = ten_go_extension_create_internal(
      go_extension, ten_string_get_raw_str(&extension_name));

  ten_string_deinit(&extension_name);

  *bridge_addr = (uintptr_t)extension;

  return cgo_error;
}

ten_extension_t *ten_go_extension_c_extension(ten_go_extension_t *self) {
  TEN_ASSERT(ten_go_extension_check_integrity(self), "Should not happen.");

  return self->c_extension;
}

void ten_go_extension_finalize(uintptr_t bridge_addr) {
  ten_go_extension_t *self = ten_go_extension_reinterpret(bridge_addr);
  TEN_ASSERT(ten_go_extension_check_integrity(self), "Should not happen.");

  ten_go_bridge_destroy_go_part(&self->bridge);
}
