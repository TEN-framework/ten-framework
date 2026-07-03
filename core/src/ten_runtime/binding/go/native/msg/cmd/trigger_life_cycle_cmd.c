//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/cmd/trigger_life_cycle/cmd.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

ten_go_error_t ten_go_cmd_create_trigger_life_cycle_cmd(uintptr_t *bridge) {
  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_shared_ptr_t *c_cmd = ten_cmd_trigger_life_cycle_create();
  TEN_ASSERT(c_cmd && ten_cmd_check_integrity(c_cmd), "Should not happen.");

  ten_go_msg_t *msg_bridge = ten_go_msg_create(c_cmd);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  *bridge = (uintptr_t)msg_bridge;
  ten_shared_ptr_destroy(c_cmd);

  return cgo_error;
}

ten_go_error_t ten_go_cmd_trigger_life_cycle_set_stage(uintptr_t bridge_addr,
                                                       const void *stage,
                                                       int stage_len) {
  ten_go_msg_t *msg_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(msg_bridge && ten_go_msg_check_integrity(msg_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_string_t stage_str;
  ten_string_init_from_c_str_with_size(&stage_str, stage, stage_len);

  bool success = ten_cmd_trigger_life_cycle_set_stage(
      ten_go_msg_c_msg(msg_bridge), ten_string_get_raw_str(&stage_str));

  if (!success) {
    ten_go_error_set(&cgo_error, TEN_ERROR_CODE_GENERIC,
                     "Failed to set stage for trigger life cycle command");
  }

  ten_string_deinit(&stage_str);

  return cgo_error;
}
