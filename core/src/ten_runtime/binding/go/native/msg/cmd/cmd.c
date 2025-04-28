//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/go/interface/ten_runtime/cmd.h"

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

ten_go_handle_t tenGoCreateCmdResult(uintptr_t);

ten_go_error_t ten_go_cmd_create_cmd(const void *name, int name_len,
                                     uintptr_t *bridge) {
  TEN_ASSERT(name && name_len > 0, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_string_t cmd_name;
  ten_string_init_from_c_str_with_size(&cmd_name, name, name_len);

  ten_shared_ptr_t *cmd =
      ten_cmd_create(ten_string_get_raw_str(&cmd_name), NULL);
  TEN_ASSERT(cmd && ten_cmd_check_integrity(cmd), "Should not happen.");

  ten_go_msg_t *msg_bridge = ten_go_msg_create(cmd);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  *bridge = (uintptr_t)msg_bridge;
  ten_shared_ptr_destroy(cmd);

  ten_string_deinit(&cmd_name);

  return cgo_error;
}

ten_go_error_t ten_go_cmd_clone(uintptr_t bridge_addr,
                                uintptr_t *cloned_bridge) {
  TEN_ASSERT(bridge_addr && cloned_bridge, "Invalid argument.");

  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_shared_ptr_t *c_cmd = ten_go_msg_c_msg(msg_bridge);
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_shared_ptr_t *cloned_c_cmd = ten_msg_clone(c_cmd, NULL);
  TEN_ASSERT(cloned_c_cmd, "Should not happen.");

  ten_go_msg_t *cloned_msg_bridge = ten_go_msg_create(cloned_c_cmd);
  TEN_ASSERT(cloned_msg_bridge, "Should not happen.");

  ten_shared_ptr_destroy(cloned_c_cmd);

  *cloned_bridge = (uintptr_t)cloned_msg_bridge;

  return cgo_error;
}

uintptr_t ten_go_cmd_create_cmd_result(int status_code, uintptr_t target_cmd) {
  TEN_ASSERT(
      status_code == TEN_STATUS_CODE_OK || status_code == TEN_STATUS_CODE_ERROR,
      "Should not happen.");

  ten_go_msg_t *target_cmd_bridge = ten_go_msg_reinterpret(target_cmd);
  TEN_ASSERT(target_cmd_bridge && ten_go_msg_check_integrity(target_cmd_bridge),
             "Should not happen.");

  TEN_STATUS_CODE code = (TEN_STATUS_CODE)status_code;

  ten_shared_ptr_t *c_cmd =
      ten_cmd_result_create_from_cmd(code, ten_go_msg_c_msg(target_cmd_bridge));
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_go_msg_t *msg_bridge = ten_go_msg_create(c_cmd);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_shared_ptr_destroy(c_cmd);

  return (uintptr_t)msg_bridge;
}

int ten_go_cmd_result_get_status_code(uintptr_t bridge_addr) {
  ten_go_msg_t *self = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_msg_check_integrity(self), "Should not happen.");

  return ten_cmd_result_get_status_code(ten_go_msg_c_msg(self));
}

ten_go_error_t ten_go_cmd_result_set_final(uintptr_t bridge_addr,
                                           bool is_final) {
  TEN_ASSERT(bridge_addr, "Invalid argument.");

  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_shared_ptr_t *c_cmd = ten_go_msg_c_msg(msg_bridge);
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success =
      ten_cmd_result_set_final(ten_go_msg_c_msg(msg_bridge), is_final, &err);

  if (!ten_error_is_success(&err)) {
    TEN_ASSERT(!success, "Should not happen.");
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  }

  ten_error_deinit(&err);
  return cgo_error;
}

ten_go_error_t ten_go_cmd_result_is_final(uintptr_t bridge_addr,
                                          bool *is_final) {
  TEN_ASSERT(bridge_addr && is_final, "Invalid argument.");

  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_shared_ptr_t *c_cmd = ten_go_msg_c_msg(msg_bridge);
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool is_final_ = ten_cmd_result_is_final(ten_go_msg_c_msg(msg_bridge), &err);

  if (!ten_error_is_success(&err)) {
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  } else {
    *is_final = is_final_;
  }

  ten_error_deinit(&err);
  return cgo_error;
}

ten_go_error_t ten_go_cmd_result_is_completed(uintptr_t bridge_addr,
                                              bool *is_completed) {
  TEN_ASSERT(bridge_addr && is_completed, "Invalid argument.");

  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_shared_ptr_t *c_cmd = ten_go_msg_c_msg(msg_bridge);
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool is_completed_ =
      ten_cmd_result_is_completed(ten_go_msg_c_msg(msg_bridge), &err);

  if (!ten_error_is_success(&err)) {
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  } else {
    *is_completed = is_completed_;
  }

  ten_error_deinit(&err);
  return cgo_error;
}

ten_go_error_t ten_go_cmd_result_clone(uintptr_t bridge_addr,
                                       uintptr_t *cloned_bridge) {
  TEN_ASSERT(bridge_addr && cloned_bridge, "Invalid argument.");

  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_shared_ptr_t *c_cmd = ten_go_msg_c_msg(msg_bridge);
  TEN_ASSERT(c_cmd, "Should not happen.");

  ten_shared_ptr_t *cloned_c_cmd = ten_msg_clone(c_cmd, NULL);
  TEN_ASSERT(cloned_c_cmd, "Should not happen.");

  ten_go_msg_t *cloned_msg_bridge = ten_go_msg_create(cloned_c_cmd);
  TEN_ASSERT(cloned_msg_bridge, "Should not happen.");

  ten_shared_ptr_destroy(cloned_c_cmd);

  *cloned_bridge = (uintptr_t)cloned_msg_bridge;

  return cgo_error;
}
