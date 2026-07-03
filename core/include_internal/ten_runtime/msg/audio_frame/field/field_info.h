//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stddef.h>

#include "include_internal/ten_runtime/msg/audio_frame/field/buf.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/bytes_per_sample.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/data_fmt.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/field.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/line_size.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/number_of_channel.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/sample_rate.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/samples_per_channel.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/timestamp.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"

#if defined(__cplusplus)
#error \
    "This file contains C99 array designated initializer, and Visual Studio C++ compiler can only support up to C89 by default, so we enable this checking to prevent any wrong inclusion of this file."
#endif

static const ten_msg_field_info_t ten_audio_frame_fields_info[] = {
    [TEN_AUDIO_FRAME_FIELD_MSGHDR] =
        {
            .field_name = NULL,
            .field_id = -1,
            .copy_field = ten_raw_msg_copy_field,
            .process_field = ten_raw_msg_process_field,
        },
    [TEN_AUDIO_FRAME_FIELD_TIMESTAMP] =
        {
            .field_name = NULL,
            .field_id = TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_TIMESTAMP,
            .copy_field = ten_audio_frame_copy_timestamp,
            .process_field = ten_audio_frame_process_timestamp,
        },
    [TEN_AUDIO_FRAME_FIELD_SAMPLE_RATE] =
        {
            .field_name = NULL,
            .field_id = TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_SAMPLE_RATE,
            .copy_field = ten_audio_frame_copy_sample_rate,
            .process_field = ten_audio_frame_process_sample_rate,
        },
    [TEN_AUDIO_FRAME_FIELD_BYTES_PER_SAMPLE] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_BYTES_PER_SAMPLE,
            .copy_field = ten_audio_frame_copy_bytes_per_sample,
            .process_field = ten_audio_frame_process_bytes_per_sample,
        },
    [TEN_AUDIO_FRAME_FIELD_SAMPLES_PER_CHANNEL] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_SAMPLES_PER_CHANNEL,
            .copy_field = ten_audio_frame_copy_samples_per_channel,
            .process_field = ten_audio_frame_process_samples_per_channel,
        },
    [TEN_AUDIO_FRAME_FIELD_NUMBER_OF_CHANNEL] =
        {
            .field_name = NULL,
            .field_id =
                TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_NUMBER_OF_CHANNEL,
            .copy_field = ten_audio_frame_copy_number_of_channel,
            .process_field = ten_audio_frame_process_number_of_channel,
        },
    [TEN_AUDIO_FRAME_FIELD_DATA_FMT] =
        {
            .field_name = NULL,
            .field_id = TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_DATA_FMT,
            .copy_field = ten_audio_frame_copy_data_fmt,
            .process_field = ten_audio_frame_process_data_fmt,
        },
    [TEN_AUDIO_FRAME_FIELD_BUF] =
        {
            .field_name = NULL,
            .field_id = TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_BUF,
            .copy_field = NULL,
            .process_field = ten_audio_frame_process_buf,
        },
    [TEN_AUDIO_FRAME_FIELD_LINE_SIZE] =
        {
            .field_name = NULL,
            .field_id = TEN_MSG_FIELD_LAST + TEN_AUDIO_FRAME_FIELD_LINE_SIZE,
            .copy_field = ten_audio_frame_copy_line_size,
            .process_field = ten_audio_frame_process_line_size,
        },
    [TEN_AUDIO_FRAME_FIELD_LAST] = {0},
};

static const size_t ten_audio_frame_fields_info_size =
    sizeof(ten_audio_frame_fields_info) /
    sizeof(ten_audio_frame_fields_info[0]);
