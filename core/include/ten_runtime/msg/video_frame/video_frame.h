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

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
typedef enum TEN_PIXEL_FMT {
  TEN_PIXEL_FMT_INVALID,

  TEN_PIXEL_FMT_RGB24,
  TEN_PIXEL_FMT_RGBA,

  TEN_PIXEL_FMT_BGR24,
  TEN_PIXEL_FMT_BGRA,

  TEN_PIXEL_FMT_I422,
  TEN_PIXEL_FMT_I420,

  TEN_PIXEL_FMT_NV21,
  TEN_PIXEL_FMT_NV12,
} TEN_PIXEL_FMT;

typedef struct ten_video_frame_t ten_video_frame_t;

TEN_RUNTIME_API ten_shared_ptr_t *ten_video_frame_create(const char *name,
                                                         ten_error_t *err);

TEN_RUNTIME_API int32_t ten_video_frame_get_width(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_video_frame_set_width(ten_shared_ptr_t *self,
                                               int32_t width);

TEN_RUNTIME_API int32_t ten_video_frame_get_height(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_video_frame_set_height(ten_shared_ptr_t *self,
                                                int32_t height);

TEN_RUNTIME_API int64_t ten_video_frame_get_timestamp(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_video_frame_set_timestamp(ten_shared_ptr_t *self,
                                                   int64_t timestamp);

TEN_RUNTIME_API bool ten_video_frame_is_eof(ten_shared_ptr_t *self);
TEN_RUNTIME_API bool ten_video_frame_set_eof(ten_shared_ptr_t *self,
                                             bool is_eof);

TEN_RUNTIME_API TEN_PIXEL_FMT
ten_video_frame_get_pixel_fmt(ten_shared_ptr_t *self);

TEN_RUNTIME_API bool ten_video_frame_set_pixel_fmt(ten_shared_ptr_t *self,
                                                   TEN_PIXEL_FMT type);

TEN_RUNTIME_API uint8_t *ten_video_frame_alloc_data(ten_shared_ptr_t *self,
                                                    size_t size);

TEN_RUNTIME_API ten_buf_t *ten_video_frame_peek_buf(ten_shared_ptr_t *self);
