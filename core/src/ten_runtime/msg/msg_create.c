//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/audio_frame/audio_frame.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/data/data.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg/video_frame/video_frame.h"
#include "ten_runtime/msg/msg.h"

void ten_raw_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
    ten_raw_cmd_destroy((ten_cmd_t *)self);
    break;

  case TEN_MSG_TYPE_CMD_RESULT:
    ten_raw_cmd_result_destroy((ten_cmd_result_t *)self);
    break;

  case TEN_MSG_TYPE_DATA:
    ten_raw_data_destroy((ten_data_t *)self);
    break;

  case TEN_MSG_TYPE_VIDEO_FRAME:
    ten_raw_video_frame_destroy((ten_video_frame_t *)self);
    break;

  case TEN_MSG_TYPE_AUDIO_FRAME:
    ten_raw_audio_frame_destroy((ten_audio_frame_t *)self);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}
