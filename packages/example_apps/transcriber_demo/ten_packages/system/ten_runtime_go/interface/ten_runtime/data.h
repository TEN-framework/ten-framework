//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdint.h>

#include "common.h"

ten_go_error_t ten_go_data_create(const void *msg_name, int msg_name_len,
                                  uintptr_t *bridge);

ten_go_error_t ten_go_data_clone(uintptr_t bridge_addr,
                                 uintptr_t *cloned_bridge);

ten_go_error_t ten_go_data_alloc_buf(uintptr_t bridge_addr, int size);

ten_go_error_t ten_go_data_lock_buf(uintptr_t bridge_addr, uint8_t **buf_addr,
                                    uint64_t *buf_size);

ten_go_error_t ten_go_data_unlock_buf(uintptr_t bridge_addr,
                                      const void *buf_addr);

ten_go_error_t ten_go_data_get_buf(uintptr_t bridge_addr, const void *buf_addr,
                                   uint64_t buf_size);

ten_go_error_t ten_go_data_get_buf_size(uintptr_t bridge_addr,
                                        uint64_t *buf_size);
