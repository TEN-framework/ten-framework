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

ten_go_error_t ten_go_audio_frame_create(const void *msg_name, int msg_name_len,
                                         uintptr_t *bridge_addr);

ten_go_error_t ten_go_audio_frame_clone(uintptr_t bridge_addr,
                                        uintptr_t *cloned_bridge);

ten_go_error_t ten_go_audio_frame_set_timestamp(uintptr_t bridge_addr,
                                                int64_t timestamp);

ten_go_error_t ten_go_audio_frame_get_timestamp(uintptr_t bridge_addr,
                                                int64_t *timestamp);

ten_go_error_t ten_go_audio_frame_set_sample_rate(uintptr_t bridge_addr,
                                                  int32_t sample_rate);

ten_go_error_t ten_go_audio_frame_get_sample_rate(uintptr_t bridge_addr,
                                                  int32_t *sample_rate);

ten_go_error_t ten_go_audio_frame_set_channel_layout(uintptr_t bridge_addr,
                                                     uint64_t channel_layout);

ten_go_error_t ten_go_audio_frame_get_channel_layout(uintptr_t bridge_addr,
                                                     uint64_t *channel_layout);

ten_go_error_t ten_go_audio_frame_set_samples_per_channel(
    uintptr_t bridge_addr, int32_t samples_per_channel);

ten_go_error_t ten_go_audio_frame_get_samples_per_channel(
    uintptr_t bridge_addr, int32_t *samples_per_channel);

ten_go_error_t ten_go_audio_frame_set_bytes_per_sample(
    uintptr_t bridge_addr, int32_t bytes_per_sample);

ten_go_error_t ten_go_audio_frame_get_bytes_per_sample(
    uintptr_t bridge_addr, int32_t *bytes_per_sample);

ten_go_error_t ten_go_audio_frame_set_number_of_channels(
    uintptr_t bridge_addr, int32_t number_of_channels);

ten_go_error_t ten_go_audio_frame_get_number_of_channels(
    uintptr_t bridge_addr, int32_t *number_of_channels);

ten_go_error_t ten_go_audio_frame_set_data_fmt(uintptr_t bridge_addr,
                                               uint32_t fmt);

ten_go_error_t ten_go_audio_frame_get_data_fmt(uintptr_t bridge_addr,
                                               uint32_t *fmt);

ten_go_error_t ten_go_audio_frame_set_line_size(uintptr_t bridge_addr,
                                                int32_t line_size);

ten_go_error_t ten_go_audio_frame_get_line_size(uintptr_t bridge_addr,
                                                int32_t *line_size);

ten_go_error_t ten_go_audio_frame_set_eof(uintptr_t bridge_addr, bool is_eof);

ten_go_error_t ten_go_audio_frame_is_eof(uintptr_t bridge_addr, bool *is_eof);

ten_go_error_t ten_go_audio_frame_alloc_buf(uintptr_t bridge_addr, int size);

ten_go_error_t ten_go_audio_frame_get_buf(uintptr_t bridge_addr,
                                          const void *buf_addr,
                                          uint64_t buf_size);

ten_go_error_t ten_go_audio_frame_lock_buf(uintptr_t bridge_addr,
                                           uint8_t **buf_addr,
                                           uint64_t *buf_size);

ten_go_error_t ten_go_audio_frame_unlock_buf(uintptr_t bridge_addr,
                                             const void *buf_addr);

ten_go_error_t ten_go_audio_frame_get_buf_size(uintptr_t bridge_addr,
                                               uint64_t *buf_size);
