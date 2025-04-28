//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdint.h>

#include "common.h"
#include "msg.h"

typedef struct ten_go_value_t ten_go_value_t;

/**
 * @brief Create a cmd based on the cmd name.
 *
 * @param cmd_name The underlying buffer of the GO string, which is passed with
 * unsafe.Pointer in GO world, so the type of @a cmd_name is void*, not char*.
 * Only the read operation is permitted. And the buffer is managed by GO, do not
 * read it after the blocking cgo call.
 *
 * @param cmd_name_len The length of the buffer.
 *
 * @param bridge The pointer to a GO variable of type uintptr, which is used to
 * store the address of a `ten_go_msg_t` pointer. This function will allocate
 * and assign the bit pattern of a pointer to an appropriate `ten_go_msg_t`
 * instance to it, which is then returned to Go.
 */
ten_go_error_t ten_go_cmd_create_cmd(const void *cmd_name, int cmd_name_len,
                                     uintptr_t *bridge);

ten_go_error_t ten_go_cmd_clone(uintptr_t bridge_addr,
                                uintptr_t *cloned_bridge);

uintptr_t ten_go_cmd_create_cmd_result(int status_code, uintptr_t target_cmd);

ten_go_error_t ten_go_cmd_result_clone(uintptr_t bridge_addr,
                                       uintptr_t *cloned_bridge);

int ten_go_cmd_result_get_status_code(uintptr_t bridge_addr);

ten_go_error_t ten_go_cmd_result_set_final(uintptr_t bridge_addr,
                                           bool is_final);

ten_go_error_t ten_go_cmd_result_is_final(uintptr_t bridge_addr,
                                          bool *is_final);

ten_go_error_t ten_go_cmd_result_is_completed(uintptr_t bridge_addr,
                                              bool *is_completed);

ten_go_handle_t ten_go_cmd_result_get_detail(uintptr_t bridge_addr);

ten_go_error_t ten_go_cmd_result_get_detail_json_and_size(
    uintptr_t bridge_addr, uintptr_t *json_str_len, const char **json_str);
