//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/go/interface/ten_runtime/data.h"

#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/msg/data/data.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/data/data.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

ten_go_error_t ten_go_data_create(const void *name, int name_len,
                                  uintptr_t *bridge) {
  TEN_ASSERT(bridge, "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_shared_ptr_t *c_data =
      ten_data_create_with_name_len(name, name_len, NULL);
  TEN_ASSERT(c_data, "Should not happen.");

  ten_go_msg_t *data_bridge = ten_go_msg_create(c_data);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Should not happen.");

  uintptr_t addr = (uintptr_t)data_bridge;
  *bridge = addr;

  // The ownership of the C message instance is transferred into the GO message
  // instance.
  ten_shared_ptr_destroy(c_data);

  return cgo_error;
}

ten_go_error_t ten_go_data_alloc_buf(uintptr_t bridge_addr, int size) {
  TEN_ASSERT(bridge_addr && size, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Invalid argument.");

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);
  uint8_t *data = ten_data_alloc_buf(c_data, size);
  if (!data) {
    ten_go_error_set(&cgo_error, TEN_ERROR_CODE_GENERIC,
                     "Failed to allocate memory");
  }

  return cgo_error;
}

ten_go_error_t ten_go_data_lock_buf(uintptr_t bridge_addr, uint8_t **buf_addr,
                                    uint64_t *buf_size) {
  TEN_ASSERT(bridge_addr && buf_size, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Invalid argument.");

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);
  uint8_t *c_data_data = ten_data_peek_buf(c_data)->data;

  ten_error_t c_err;
  ten_error_init(&c_err);

  if (!ten_msg_add_locked_res_buf(c_data, c_data_data, &c_err)) {
    ten_go_error_set(&cgo_error, ten_error_code(&c_err),
                     ten_error_message(&c_err));
  } else {
    *buf_addr = c_data_data;
    *buf_size = ten_data_peek_buf(c_data)->size;
  }

  ten_error_deinit(&c_err);

  return cgo_error;
}

ten_go_error_t ten_go_data_unlock_buf(uintptr_t bridge_addr,
                                      const void *buf_addr) {
  TEN_ASSERT(bridge_addr && buf_addr, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Invalid argument.");

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);

  ten_error_t c_err;
  ten_error_init(&c_err);

  bool result = ten_msg_remove_locked_res_buf(c_data, buf_addr, &c_err);
  if (!result) {
    ten_go_error_set(&cgo_error, ten_error_code(&c_err),
                     ten_error_message(&c_err));
  }

  ten_error_deinit(&c_err);

  return cgo_error;
}

ten_go_error_t ten_go_data_get_buf(uintptr_t bridge_addr, const void *buf_addr,
                                   uint64_t buf_size) {
  TEN_ASSERT(bridge_addr && buf_addr && buf_size > 0, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Invalid argument.");

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);
  uint64_t size = ten_data_peek_buf(c_data)->size;
  if (buf_size < size) {
    ten_go_error_set(&cgo_error, TEN_ERROR_CODE_GENERIC,
                     "buffer is not enough");
  } else {
    uint8_t *data = ten_data_peek_buf(c_data)->data;
    memcpy((void *)buf_addr, data, size);
  }

  return cgo_error;
}

ten_go_error_t ten_go_data_get_buf_size(uintptr_t bridge_addr,
                                        uint64_t *buf_size) {
  TEN_ASSERT(bridge_addr, "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Invalid argument.");

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);
  uint64_t size = ten_data_peek_buf(c_data)->size;

  *buf_size = size;

  return cgo_error;
}

ten_go_error_t ten_go_data_clone(uintptr_t bridge_addr,
                                 uintptr_t *cloned_bridge) {
  TEN_ASSERT(bridge_addr && cloned_bridge, "Invalid argument.");

  ten_go_msg_t *data_bridge = ten_go_msg_reinterpret(bridge_addr);
  TEN_ASSERT(data_bridge && ten_go_msg_check_integrity(data_bridge),
             "Should not happen.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_shared_ptr_t *c_data = ten_go_msg_c_msg(data_bridge);
  ten_shared_ptr_t *cloned_c_data = ten_msg_clone(c_data, NULL);
  TEN_ASSERT(cloned_c_data, "Should not happen.");

  ten_go_msg_t *cloned_msg_bridge = ten_go_msg_create(cloned_c_data);
  TEN_ASSERT(cloned_msg_bridge, "Should not happen.");

  ten_shared_ptr_destroy(cloned_c_data);

  *cloned_bridge = (uintptr_t)cloned_msg_bridge;

  return cgo_error;
}
