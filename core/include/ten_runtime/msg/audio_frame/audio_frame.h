//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>
#include <stdint.h>

#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
typedef enum TEN_AUDIO_FRAME_DATA_FMT {
  TEN_AUDIO_FRAME_DATA_FMT_INVALID,

  // Packet format in FFmpeg. Ex: ABABABAB
  TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE,

  // Planar format in FFmpeg. Ex: AAAABBBB
  TEN_AUDIO_FRAME_DATA_FMT_NON_INTERLEAVE,
} TEN_AUDIO_FRAME_DATA_FMT;

typedef struct ten_audio_frame_t ten_audio_frame_t;

TEN_RUNTIME_API ten_shared_ptr_t *ten_audio_frame_create(const char *name,
                                                         ten_error_t *err);

TEN_RUNTIME_API int64_t ten_audio_frame_get_timestamp(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_timestamp(ten_shared_ptr_t *self,
                                                   int64_t timestamp);

TEN_RUNTIME_API int32_t ten_audio_frame_get_sample_rate(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_sample_rate(ten_shared_ptr_t *self,
                                                     int32_t sample_rate);

TEN_RUNTIME_API uint64_t
ten_audio_frame_get_channel_layout(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_channel_layout(
    ten_shared_ptr_t *self, uint64_t channel_layout);

TEN_RUNTIME_API bool ten_audio_frame_is_eof(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_eof(ten_shared_ptr_t *self,
                                             bool is_eof);

TEN_RUNTIME_API int32_t
ten_audio_frame_get_samples_per_channel(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_samples_per_channel(
    ten_shared_ptr_t *self, int32_t samples_per_channel);

TEN_RUNTIME_API int32_t ten_audio_frame_get_line_size(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_line_size(ten_shared_ptr_t *self,
                                                   int32_t line_size);

TEN_RUNTIME_API int32_t
ten_audio_frame_get_bytes_per_sample(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_bytes_per_sample(
    ten_shared_ptr_t *self, int32_t size);

TEN_RUNTIME_API int32_t
ten_audio_frame_get_number_of_channel(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_number_of_channel(
    ten_shared_ptr_t *self, int32_t number);

TEN_RUNTIME_API TEN_AUDIO_FRAME_DATA_FMT
ten_audio_frame_get_data_fmt(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_audio_frame_set_data_fmt(
    ten_shared_ptr_t *self, TEN_AUDIO_FRAME_DATA_FMT data_fmt);

TEN_RUNTIME_API uint8_t *ten_audio_frame_alloc_buf(ten_shared_ptr_t *self,
                                                   size_t size);

TEN_RUNTIME_API ten_buf_t *ten_audio_frame_peek_buf(ten_shared_ptr_t *self);
