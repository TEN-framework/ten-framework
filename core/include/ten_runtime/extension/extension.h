//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_t ten_extension_t;
typedef struct ten_env_t ten_env_t;
typedef struct ten_metadata_info_t ten_metadata_info_t;

typedef void (*ten_extension_on_configure_func_t)(ten_extension_t *self,
                                                  ten_env_t *ten_env);

typedef void (*ten_extension_on_init_func_t)(ten_extension_t *self,
                                             ten_env_t *ten_env);

typedef void (*ten_extension_on_start_func_t)(ten_extension_t *self,
                                              ten_env_t *ten_env);

typedef void (*ten_extension_on_stop_func_t)(ten_extension_t *self,
                                             ten_env_t *ten_env);

typedef void (*ten_extension_on_deinit_func_t)(ten_extension_t *self,
                                               ten_env_t *ten_env);

typedef void (*ten_extension_on_cmd_func_t)(ten_extension_t *self,
                                            ten_env_t *ten_env,
                                            ten_shared_ptr_t *cmd);

typedef void (*ten_extension_on_data_func_t)(ten_extension_t *self,
                                             ten_env_t *ten_env,
                                             ten_shared_ptr_t *data);

typedef void (*ten_extension_on_audio_frame_func_t)(ten_extension_t *self,
                                                    ten_env_t *ten_env,
                                                    ten_shared_ptr_t *frame);

typedef void (*ten_extension_on_video_frame_func_t)(ten_extension_t *self,
                                                    ten_env_t *ten_env,
                                                    ten_shared_ptr_t *frame);

TEN_RUNTIME_API bool ten_extension_check_integrity(ten_extension_t *self,
                                                   bool check_thread);

TEN_RUNTIME_API ten_extension_t *ten_extension_create(
    const char *name, ten_extension_on_configure_func_t on_configure,
    ten_extension_on_init_func_t on_init,
    ten_extension_on_start_func_t on_start,
    ten_extension_on_stop_func_t on_stop,
    ten_extension_on_deinit_func_t on_deinit,
    ten_extension_on_cmd_func_t on_cmd, ten_extension_on_data_func_t on_data,
    ten_extension_on_audio_frame_func_t on_audio_frame,
    ten_extension_on_video_frame_func_t on_video_frame, void *user_data);

TEN_RUNTIME_API void ten_extension_destroy(ten_extension_t *self);

TEN_RUNTIME_API ten_env_t *ten_extension_get_ten_env(ten_extension_t *self);
