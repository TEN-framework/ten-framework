//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_utils/container/list.h"

typedef struct ten_msg_t ten_msg_t;
typedef struct ten_error_t ten_error_t;

TEN_RUNTIME_PRIVATE_API void ten_audio_frame_copy_data_fmt(
    ten_msg_t *self, ten_msg_t *src, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API const char *ten_audio_frame_data_fmt_to_string(
    TEN_AUDIO_FRAME_DATA_FMT data_fmt);

TEN_RUNTIME_PRIVATE_API TEN_AUDIO_FRAME_DATA_FMT
ten_audio_frame_data_fmt_from_string(const char *data_fmt_str);

TEN_RUNTIME_PRIVATE_API bool ten_audio_frame_process_data_fmt(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
