//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/audio_frame/field/sample_rate.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/audio_frame/audio_frame.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_audio_frame_copy_sample_rate(
    ten_msg_t *self, ten_msg_t *src,
    TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && src && ten_raw_msg_check_integrity(src) &&
                 ten_raw_msg_get_type(src) == TEN_MSG_TYPE_AUDIO_FRAME,
             "Should not happen.");

  ten_raw_audio_frame_set_sample_rate(
      (ten_audio_frame_t *)self,
      ten_raw_audio_frame_get_sample_rate((ten_audio_frame_t *)src));
}

bool ten_audio_frame_process_sample_rate(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t sample_rate_field;
  ten_msg_field_process_data_init(&sample_rate_field, TEN_STR_SAMPLE_RATE,
                                  &((ten_audio_frame_t *)self)->sample_rate,
                                  false);

  bool rc = cb(self, &sample_rate_field, user_data, err);

  return rc;
}
