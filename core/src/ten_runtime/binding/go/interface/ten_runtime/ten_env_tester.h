//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

#define TEN_GO_TEN_ENV_TESTER_SIGNATURE 0x9159C741BA4A16D3U

void ten_go_ten_env_tester_finalize(uintptr_t bridge_addr);

ten_go_error_t ten_go_ten_env_tester_on_start_done(uintptr_t bridge_addr);

ten_go_error_t ten_go_ten_env_tester_on_stop_done(uintptr_t bridge_addr);

ten_go_error_t ten_go_ten_env_tester_on_deinit_done(uintptr_t bridge_addr);

ten_go_error_t ten_go_ten_env_tester_send_cmd(uintptr_t bridge_addr,
                                              uintptr_t cmd_bridge_addr,
                                              ten_go_handle_t handler_id,
                                              bool is_ex);

ten_go_error_t ten_go_ten_env_tester_send_data(uintptr_t bridge_addr,
                                               uintptr_t data_bridge_addr,
                                               ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_tester_send_audio_frame(
    uintptr_t bridge_addr, uintptr_t audio_frame_bridge_addr,
    ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_tester_send_video_frame(
    uintptr_t bridge_addr, uintptr_t video_frame_bridge_addr,
    ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_tester_return_result(
    uintptr_t bridge_addr, uintptr_t cmd_result_bridge_addr,
    ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_tester_stop_test(uintptr_t bridge_addr,
                                               uint32_t error_code,
                                               void *error_message,
                                               uint32_t error_message_size);

ten_go_error_t ten_go_ten_env_tester_log(
    uintptr_t bridge_addr, int level, const void *func_name, int func_name_len,
    const void *file_name, int file_name_len, int line_no, const void *msg,
    int msg_len, const void *category, int category_len);
