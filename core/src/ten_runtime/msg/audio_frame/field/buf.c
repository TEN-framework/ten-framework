//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/audio_frame/field/buf.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/audio_frame/audio_frame.h"
#include "include_internal/ten_runtime/msg/msg.h"

bool ten_audio_frame_process_buf(ten_msg_t *self,
                                 ten_raw_msg_process_one_field_func_t cb,
                                 void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t data_fmt_field;
  ten_msg_field_process_data_init(&data_fmt_field, TEN_STR_BUF,
                                  &((ten_audio_frame_t *)self)->buf, false);

  bool rc = cb(self, &data_fmt_field, user_data, err);

  return rc;
}
