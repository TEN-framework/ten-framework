//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/test/extension_tester.h"

#include <stdint.h>

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/test/env_tester.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

extern void tenGoExtensionTesterOnStart(ten_go_handle_t go_extension_tester,
                                        ten_go_handle_t go_ten_env_tester);

extern void tenGoExtensionTesterOnCmd(ten_go_handle_t go_extension_tester,
                                      ten_go_handle_t go_ten_env_tester,
                                      uintptr_t cmd_bridge_addr);

extern void tenGoExtensionTesterOnData(ten_go_handle_t go_extension_tester,
                                       ten_go_handle_t go_ten_env_tester,
                                       uintptr_t data_bridge_addr);

extern void tenGoExtensionTesterOnAudioFrame(
    ten_go_handle_t go_extension_tester, ten_go_handle_t go_ten_env_tester,
    uintptr_t audio_frame_bridge_addr);

extern void tenGoExtensionTesterOnVideoFrame(
    ten_go_handle_t go_extension_tester, ten_go_handle_t go_ten_env_tester,
    uintptr_t video_frame_bridge_addr);

bool ten_go_extension_tester_check_integrity(ten_go_extension_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      TEN_GO_EXTENSION_TESTER_SIGNATURE) {
    return false;
  }

  return true;
}

ten_go_extension_tester_t *ten_go_extension_tester_reinterpret(
    uintptr_t bridge_addr) {
  TEN_ASSERT(bridge_addr, "Invalid argument.");

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  ten_go_extension_tester_t *self = (ten_go_extension_tester_t *)bridge_addr;
  TEN_ASSERT(ten_go_extension_tester_check_integrity(self),
             "Invalid argument.");

  return self;
}

ten_go_handle_t ten_go_extension_tester_go_handle(
    ten_go_extension_tester_t *self) {
  TEN_ASSERT(ten_go_extension_tester_check_integrity(self),
             "Should not happen.");

  return self->bridge.go_instance;
}

static void ten_go_extension_tester_bridge_destroy(
    ten_go_extension_tester_t *self) {
  TEN_ASSERT(ten_go_extension_tester_check_integrity(self),
             "Should not happen.");

  ten_extension_tester_t *c_extension_tester = self->c_extension_tester;
  TEN_ASSERT(c_extension_tester, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In TEN world, the destroy operation should be performed in
  // any threads.
  TEN_ASSERT(ten_extension_tester_check_integrity(c_extension_tester, false),
             "Invalid use of extension_tester %p.", c_extension_tester);

  ten_extension_tester_destroy(c_extension_tester);
  TEN_FREE(self);
}

static void proxy_on_start(ten_extension_tester_t *self,
                           ten_env_tester_t *ten_env_tester) {
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(ten_extension_tester_get_ten_env_tester(self) == ten_env_tester,
             "Should not happen.");

  ten_go_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(extension_tester_bridge),
             "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);

  tenGoExtensionTesterOnStart(
      ten_go_extension_tester_go_handle(extension_tester_bridge),
      ten_go_ten_env_tester_go_handle(ten_env_tester_bridge));
}

static void proxy_on_cmd(ten_extension_tester_t *self,
                         ten_env_tester_t *ten_env_tester,
                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(ten_extension_tester_get_ten_env_tester(self) == ten_env_tester,
             "Should not happen.");
  TEN_ASSERT(cmd && ten_cmd_check_integrity(cmd), "Should not happen.");

  ten_go_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(extension_tester_bridge),
             "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);

  ten_go_msg_t *msg_bridge = ten_go_msg_create(cmd);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionTesterOnCmd(
      ten_go_extension_tester_go_handle(extension_tester_bridge),
      ten_go_ten_env_tester_go_handle(ten_env_tester_bridge), msg_bridge_addr);
}

static void proxy_on_data(ten_extension_tester_t *self,
                          ten_env_tester_t *ten_env_tester,
                          ten_shared_ptr_t *data) {
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(ten_extension_tester_get_ten_env_tester(self) == ten_env_tester,
             "Should not happen.");
  TEN_ASSERT(data && ten_msg_check_integrity(data), "Should not happen.");

  ten_go_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(extension_tester_bridge),
             "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);

  ten_go_msg_t *msg_bridge = ten_go_msg_create(data);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionTesterOnData(
      ten_go_extension_tester_go_handle(extension_tester_bridge),
      ten_go_ten_env_tester_go_handle(ten_env_tester_bridge), msg_bridge_addr);
}

static void proxy_on_audio_frame(ten_extension_tester_t *self,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(ten_extension_tester_get_ten_env_tester(self) == ten_env_tester,
             "Should not happen.");
  TEN_ASSERT(audio_frame && ten_msg_check_integrity(audio_frame),
             "Should not happen.");

  ten_go_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(extension_tester_bridge),
             "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);

  ten_go_msg_t *msg_bridge = ten_go_msg_create(audio_frame);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionTesterOnAudioFrame(
      ten_go_extension_tester_go_handle(extension_tester_bridge),
      ten_go_ten_env_tester_go_handle(ten_env_tester_bridge), msg_bridge_addr);
}

static void proxy_on_video_frame(ten_extension_tester_t *self,
                                 ten_env_tester_t *ten_env_tester,
                                 ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(ten_extension_tester_get_ten_env_tester(self) == ten_env_tester,
             "Should not happen.");
  TEN_ASSERT(video_frame && ten_msg_check_integrity(video_frame),
             "Should not happen.");

  ten_go_extension_tester_t *extension_tester_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)self);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(extension_tester_bridge),
             "Should not happen.");

  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_go_ten_env_tester_wrap(ten_env_tester);

  ten_go_msg_t *msg_bridge = ten_go_msg_create(video_frame);
  uintptr_t msg_bridge_addr = (uintptr_t)msg_bridge;

  tenGoExtensionTesterOnVideoFrame(
      ten_go_extension_tester_go_handle(extension_tester_bridge),
      ten_go_ten_env_tester_go_handle(ten_env_tester_bridge), msg_bridge_addr);
}

ten_go_error_t ten_go_extension_tester_create(
    ten_go_handle_t go_extension_tester, uintptr_t *bridge_addr) {
  TEN_ASSERT(go_extension_tester > 0 && bridge_addr, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_extension_tester_t *extension_tester =
      ten_go_extension_tester_create_internal(go_extension_tester);

  *bridge_addr = (uintptr_t)extension_tester;

  return cgo_error;
}

void ten_go_extension_tester_finalize(uintptr_t bridge_addr) {
  ten_go_extension_tester_t *self =
      ten_go_extension_tester_reinterpret(bridge_addr);
  TEN_ASSERT(ten_go_extension_tester_check_integrity(self),
             "Should not happen.");

  ten_go_bridge_destroy_go_part(&self->bridge);
}

ten_go_extension_tester_t *ten_go_extension_tester_create_internal(
    ten_go_handle_t go_extension_tester) {
  ten_go_extension_tester_t *extension_tester_bridge =
      (ten_go_extension_tester_t *)TEN_MALLOC(
          sizeof(ten_go_extension_tester_t));
  TEN_ASSERT(extension_tester_bridge, "Failed to allocate memory.");

  ten_signature_set(&extension_tester_bridge->signature,
                    TEN_GO_EXTENSION_TESTER_SIGNATURE);
  extension_tester_bridge->bridge.go_instance = go_extension_tester;

  extension_tester_bridge->bridge.sp_ref_by_go = ten_shared_ptr_create(
      extension_tester_bridge, ten_go_extension_tester_bridge_destroy);
  extension_tester_bridge->bridge.sp_ref_by_c = NULL;

  extension_tester_bridge->c_extension_tester = ten_extension_tester_create(
      NULL, proxy_on_start, NULL, NULL, proxy_on_cmd, proxy_on_data,
      proxy_on_audio_frame, proxy_on_video_frame);

  ten_binding_handle_set_me_in_target_lang(
      &extension_tester_bridge->c_extension_tester->binding_handle,
      extension_tester_bridge);

  return extension_tester_bridge;
}
