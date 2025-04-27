//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_runtime/ten_env/internal/send.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/log/log.h"

typedef struct ten_env_tester_t ten_env_tester_t;

TEN_RUNTIME_API bool ten_env_tester_on_init_done(ten_env_tester_t *self,
                                                 ten_error_t *err);

TEN_RUNTIME_API bool ten_env_tester_on_start_done(ten_env_tester_t *self,
                                                  ten_error_t *err);

TEN_RUNTIME_API bool ten_env_tester_on_stop_done(ten_env_tester_t *self,
                                                 ten_error_t *err);

TEN_RUNTIME_API bool ten_env_tester_on_deinit_done(ten_env_tester_t *self,
                                                   ten_error_t *err);

typedef void (*ten_env_tester_transfer_msg_result_handler_func_t)(
    ten_env_tester_t *self, ten_shared_ptr_t *cmd_result, void *user_data,
    ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_send_cmd(
    ten_env_tester_t *self, ten_shared_ptr_t *cmd,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_env_send_cmd_options_t *options, ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_send_data(
    ten_env_tester_t *self, ten_shared_ptr_t *data,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_send_audio_frame(
    ten_env_tester_t *self, ten_shared_ptr_t *audio_frame,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_send_video_frame(
    ten_env_tester_t *self, ten_shared_ptr_t *video_frame,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_return_result(
    ten_env_tester_t *self, ten_shared_ptr_t *result,
    ten_env_tester_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_stop_test(ten_env_tester_t *self,
                                              ten_error_t *error);

TEN_RUNTIME_API bool ten_env_tester_log(ten_env_tester_t *self,
                                        TEN_LOG_LEVEL level,
                                        const char *func_name,
                                        const char *file_name, size_t line_no,
                                        const char *msg, ten_error_t *error);
