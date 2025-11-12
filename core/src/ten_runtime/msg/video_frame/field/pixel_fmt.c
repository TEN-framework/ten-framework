//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/video_frame/field/pixel_fmt.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg/video_frame/video_frame.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

const char *ten_video_frame_pixel_fmt_to_string(const TEN_PIXEL_FMT pixel_fmt) {
  switch (pixel_fmt) {
  case TEN_PIXEL_FMT_RGB24:
    return "rgb24";
  case TEN_PIXEL_FMT_RGBA:
    return "rgba";
  case TEN_PIXEL_FMT_BGR24:
    return "bgr24";
  case TEN_PIXEL_FMT_BGRA:
    return "bgra";
  case TEN_PIXEL_FMT_I420:
    return "i420";
  case TEN_PIXEL_FMT_I422:
    return "i422";
  case TEN_PIXEL_FMT_NV21:
    return "nv21";
  case TEN_PIXEL_FMT_NV12:
    return "nv12";
  default:
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }
}

TEN_PIXEL_FMT ten_video_frame_pixel_fmt_from_string(const char *pixel_fmt_str) {
  if (!strcmp(pixel_fmt_str, "rgb24")) {
    return TEN_PIXEL_FMT_RGB24;
  } else if (!strcmp(pixel_fmt_str, "rgba")) {
    return TEN_PIXEL_FMT_RGBA;
  } else if (!strcmp(pixel_fmt_str, "bgr24")) {
    return TEN_PIXEL_FMT_BGR24;
  } else if (!strcmp(pixel_fmt_str, "bgra")) {
    return TEN_PIXEL_FMT_BGRA;
  } else if (!strcmp(pixel_fmt_str, "i420")) {
    return TEN_PIXEL_FMT_I420;
  } else if (!strcmp(pixel_fmt_str, "i422")) {
    return TEN_PIXEL_FMT_I422;
  } else if (!strcmp(pixel_fmt_str, "nv21")) {
    return TEN_PIXEL_FMT_NV21;
  } else if (!strcmp(pixel_fmt_str, "nv12")) {
    return TEN_PIXEL_FMT_NV12;
  } else {
    TEN_ASSERT(0, "Should not happen.");
    return TEN_PIXEL_FMT_INVALID;
  }
}

void ten_video_frame_copy_pixel_fmt(ten_msg_t *self, ten_msg_t *src,
                                    TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && src && ten_raw_msg_check_integrity(src) &&
                 ten_raw_msg_get_type(src) == TEN_MSG_TYPE_VIDEO_FRAME,
             "Should not happen.");

  ten_raw_video_frame_set_pixel_fmt(
      (ten_video_frame_t *)self,
      ten_raw_video_frame_get_pixel_fmt((ten_video_frame_t *)src));
}

bool ten_video_frame_process_pixel_fmt(ten_msg_t *self,
                                       ten_raw_msg_process_one_field_func_t cb,
                                       void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t pixel_fmt_field;
  ten_msg_field_process_data_init(&pixel_fmt_field, TEN_STR_PIXEL_FMT,
                                  &((ten_video_frame_t *)self)->pixel_fmt,
                                  false);

  bool rc = cb(self, &pixel_fmt_field, user_data, err);

  return rc;
}