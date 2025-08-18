//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

ten_go_error_t ten_go_cmd_create_start_graph_cmd(uintptr_t *bridge) {
  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_shared_ptr_t *c_cmd = ten_cmd_start_graph_create();
  TEN_ASSERT(c_cmd && ten_cmd_check_integrity(c_cmd), "Should not happen.");

  ten_go_msg_t *msg_bridge = ten_go_msg_create(c_cmd);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  *bridge = (uintptr_t)msg_bridge;
  ten_shared_ptr_destroy(c_cmd);

  return cgo_error;
}

ten_go_error_t ten_go_cmd_start_graph_set_predefined_graph_name(
    uintptr_t bridge_addr, const void *predefined_graph_name,
    int predefined_graph_name_len) {
  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_string_t predefined_graph_name_str;
  ten_string_init_from_c_str_with_size(&predefined_graph_name_str,
                                       predefined_graph_name,
                                       predefined_graph_name_len);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_cmd_start_graph_set_predefined_graph_name(
      ten_go_msg_c_msg(msg_bridge),
      ten_string_get_raw_str(&predefined_graph_name_str), &err);

  if (!success) {
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  }

  ten_error_deinit(&err);
  ten_string_deinit(&predefined_graph_name_str);

  return cgo_error;
}

ten_go_error_t ten_go_cmd_start_graph_set_graph_from_json_bytes(
    uintptr_t bridge_addr, const void *json_bytes, int json_bytes_len) {
  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_string_t json_str_str;
  ten_string_init_from_c_str_with_size(&json_str_str, json_bytes,
                                       json_bytes_len);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_cmd_start_graph_set_graph_from_json_str(
      ten_go_msg_c_msg(msg_bridge), ten_string_get_raw_str(&json_str_str),
      &err);

  if (!success) {
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  }

  ten_error_deinit(&err);
  ten_string_deinit(&json_str_str);

  return cgo_error;
}

ten_go_error_t ten_go_cmd_start_graph_set_long_running_mode(
    uintptr_t bridge_addr, bool long_running_mode) {
  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_cmd_start_graph_set_long_running_mode(
      ten_go_msg_c_msg(msg_bridge), long_running_mode, &err);

  if (!success) {
    ten_go_error_set(&cgo_error, ten_error_code(&err), ten_error_message(&err));
  }

  ten_error_deinit(&err);

  return cgo_error;
}
