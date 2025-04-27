//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/test/env_tester.h"

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env_tester.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

bool ten_go_ten_env_tester_check_integrity(ten_go_ten_env_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_GO_TEN_ENV_TESTER_SIGNATURE) {
    return false;
  }

  return true;
}

ten_go_ten_env_tester_t *ten_go_ten_env_tester_reinterpret(
    uintptr_t bridge_addr) {
  TEN_ASSERT(bridge_addr, "Should not happen.");

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  ten_go_ten_env_tester_t *self = (ten_go_ten_env_tester_t *)bridge_addr;
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  return self;
}

static void ten_go_ten_env_tester_destroy(ten_go_ten_env_tester_t *self) {
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  TEN_FREE(self);
}

static void ten_go_ten_env_tester_destroy_c_part(void *ten_env_tester_bridge_) {
  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      (ten_go_ten_env_tester_t *)ten_env_tester_bridge_;
  TEN_ASSERT(ten_env_tester_bridge &&
                 ten_go_ten_env_tester_check_integrity(ten_env_tester_bridge),
             "Should not happen.");
  ten_env_tester_bridge->c_ten_env_tester = NULL;
  ten_go_bridge_destroy_c_part(&ten_env_tester_bridge->bridge);

  // Remove the Go ten_env object from the global map.
  tenGoDestroyTenEnv(ten_env_tester_bridge->bridge.go_instance);
}

static void ten_go_ten_env_tester_detach_c_part(void *ten_env_tester_bridge_) {
  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      (ten_go_ten_env_tester_t *)ten_env_tester_bridge_;
  TEN_ASSERT(ten_env_tester_bridge &&
                 ten_go_ten_env_tester_check_integrity(ten_env_tester_bridge),
             "Should not happen.");

  ten_env_tester_bridge->c_ten_env_tester = NULL;
}

ten_go_ten_env_tester_t *ten_go_ten_env_tester_wrap(
    ten_env_tester_t *c_ten_env_tester) {
  ten_go_ten_env_tester_t *ten_env_tester_bridge =
      ten_binding_handle_get_me_in_target_lang(
          (ten_binding_handle_t *)c_ten_env_tester);
  if (ten_env_tester_bridge) {
    return ten_env_tester_bridge;
  }

  ten_env_tester_bridge =
      (ten_go_ten_env_tester_t *)TEN_MALLOC(sizeof(ten_go_ten_env_tester_t));
  TEN_ASSERT(ten_env_tester_bridge, "Failed to allocate memory.");

  ten_signature_set(&ten_env_tester_bridge->signature,
                    TEN_GO_TEN_ENV_TESTER_SIGNATURE);

  uintptr_t bridge_addr = (uintptr_t)ten_env_tester_bridge;
  TEN_ASSERT(bridge_addr, "Should not happen.");

  ten_env_tester_bridge->bridge.go_instance =
      tenGoCreateTenEnvTester(bridge_addr);

  // C ten hold one reference of ten bridge.
  ten_env_tester_bridge->bridge.sp_ref_by_c = ten_shared_ptr_create(
      ten_env_tester_bridge, ten_go_ten_env_tester_destroy);
  ten_env_tester_bridge->bridge.sp_ref_by_go =
      ten_shared_ptr_clone(ten_env_tester_bridge->bridge.sp_ref_by_c);

  ten_env_tester_bridge->c_ten_env_tester = c_ten_env_tester;

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)c_ten_env_tester, ten_env_tester_bridge);

  ten_env_tester_set_destroy_handler_in_target_lang(
      c_ten_env_tester, ten_go_ten_env_tester_destroy_c_part);
  ten_env_tester_set_close_handler_in_target_lang(
      c_ten_env_tester, ten_go_ten_env_tester_detach_c_part);

  return ten_env_tester_bridge;
}

ten_go_handle_t ten_go_ten_env_tester_go_handle(ten_go_ten_env_tester_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  return self->bridge.go_instance;
}

void ten_go_ten_env_tester_finalize(uintptr_t bridge_addr) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_bridge_destroy_go_part(&self->bridge);
}

void ten_go_ten_env_tester_on_start_done(uintptr_t bridge_addr) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");
}

ten_go_error_t ten_go_ten_env_tester_send_cmd(uintptr_t bridge_addr,
                                              uintptr_t cmd_bridge_addr,
                                              ten_go_handle_t handler_id) {
  ten_go_ten_env_tester_t *self =
      ten_go_ten_env_tester_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_tester_check_integrity(self),
             "Should not happen.");

  ten_go_msg_t *cmd = ten_go_msg_reinterpret(cmd_bridge_addr);
  TEN_ASSERT(cmd && ten_go_msg_check_integrity(cmd), "Should not happen.");
  TEN_ASSERT(ten_go_msg_c_msg(cmd), "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  return cgo_error;
}
