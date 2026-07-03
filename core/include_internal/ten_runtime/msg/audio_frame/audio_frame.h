//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdint.h>

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"

#define TEN_AUDIO_FRAME_SIGNATURE 0x356C118642A0FD8FU
#define TEN_AUDIO_FRAME_MAX_DATA_CNT 8

typedef struct ten_audio_frame_t {
  ten_msg_t msg_hdr;
  ten_signature_t signature;

  ten_value_t timestamp;    // int64. The timestamp (ms) of the audio frame.
  ten_value_t sample_rate;  // int32. The sample rate (Hz) of the pcm data.
  ten_value_t bytes_per_sample;     // int32. 1,2,4,8 bytes
  ten_value_t samples_per_channel;  // int32. The number of samples per channel.
  ten_value_t number_of_channel;    // int32. The channel number.

  // channel layout ID of FFmpeg. Please see 'channel_layout_map' of
  // https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/channel_layout.c for
  // the values of the channel layout ID.
  // If this audio frame is from one FFmpeg software re-sampler and will be
  // consumed by another FFmpeg software re-sampler, you need to remember the
  // channel layout of the audio frame, so that it could be used by those
  // software re-sampler. However, if the audio frame is nothing to do with the
  // FFmpeg software re-sampler, you could just ignore this field.
  ten_value_t channel_layout;  // uint64

  ten_value_t data_fmt;  // int32 (TEN_AUDIO_FRAME_DATA_FMT). Format of `data`.

  ten_value_t buf;  // buf

  // TODO(Liu): Add data size info for each channel.
  //
  // line_size is the size of data[i] if data[i] is not NULL, i is from 0 to
  // (TEN_AUDIO_FRAME_MAX_DATA_CNT-1).
  // - If "format" is interleave, only data[0] is used, and line_size is the
  //   size of the memory space pointed by data[0], and it should be equal to
  //   "bytes_per_sample * samples_per_channel * number_of_channel".
  // - If "format" is non-interleave,
  //   data[0]~data[TEN_AUDIO_FRAME_MAX_DATA_CNT-1] might be used, and line_size
  //   is the size of each memory space pointed to by data[i] if data[i] is not
  //   NULL, i is from 0 to (TEN_AUDIO_FRAME_MAX_DATA_CNT-1), and it should be
  //   equal to "bytes_per_sample * samples_per_channel".
  ten_value_t line_size;  // int32

  ten_value_t is_eof;  // bool
} ten_audio_frame_t;

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *ten_audio_frame_create_empty(void);

TEN_RUNTIME_API ten_shared_ptr_t *ten_audio_frame_create_with_name_len(
    const char *name, size_t name_len, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_raw_audio_frame_destroy(
    ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API ten_msg_t *ten_raw_audio_frame_as_msg_clone(
    ten_msg_t *self, ten_list_t *excluded_field_ids);

TEN_RUNTIME_PRIVATE_API int32_t
ten_raw_audio_frame_get_samples_per_channel(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API ten_buf_t *ten_raw_audio_frame_peek_buf(
    ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API int32_t
ten_raw_audio_frame_get_sample_rate(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API uint64_t
ten_raw_audio_frame_get_channel_layout(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_is_eof(
    ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API int32_t
ten_raw_audio_frame_get_line_size(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API int32_t
ten_raw_audio_frame_get_bytes_per_sample(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API int32_t
ten_raw_audio_frame_get_number_of_channel(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API TEN_AUDIO_FRAME_DATA_FMT
ten_raw_audio_frame_get_data_fmt(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API int64_t
ten_raw_audio_frame_get_timestamp(ten_audio_frame_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_samples_per_channel(
    ten_audio_frame_t *self, int32_t samples_per_channel);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_sample_rate(
    ten_audio_frame_t *self, int32_t sample_rate);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_channel_layout(
    ten_audio_frame_t *self, uint64_t channel_layout);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_eof(
    ten_audio_frame_t *self, bool is_eof);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_line_size(
    ten_audio_frame_t *self, int32_t line_size);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_bytes_per_sample(
    ten_audio_frame_t *self, int32_t bytes_per_sample);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_number_of_channel(
    ten_audio_frame_t *self, int32_t number);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_data_fmt(
    ten_audio_frame_t *self, TEN_AUDIO_FRAME_DATA_FMT data_fmt);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_set_timestamp(
    ten_audio_frame_t *self, int64_t timestamp);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_raw_audio_frame_peek_ten_property(
    ten_msg_t *self, ten_list_t *paths, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_raw_audio_frame_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);
