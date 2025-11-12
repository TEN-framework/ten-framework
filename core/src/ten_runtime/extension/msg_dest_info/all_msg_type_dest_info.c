//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_dest_info/all_msg_type_dest_info.h"

#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "ten_utils/macro/check.h"

void ten_all_msg_type_dest_info_init(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_hashtable_init(
      &self->cmd, offsetof(ten_msg_dest_info_t, hh_in_all_msg_type_dest_info));
  ten_hashtable_init(
      &self->data, offsetof(ten_msg_dest_info_t, hh_in_all_msg_type_dest_info));
  ten_hashtable_init(
      &self->audio_frame,
      offsetof(ten_msg_dest_info_t, hh_in_all_msg_type_dest_info));
  ten_hashtable_init(
      &self->video_frame,
      offsetof(ten_msg_dest_info_t, hh_in_all_msg_type_dest_info));
}

void ten_all_msg_type_dest_info_deinit(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_hashtable_deinit(&self->cmd);
  ten_hashtable_deinit(&self->data);
  ten_hashtable_deinit(&self->audio_frame);
  ten_hashtable_deinit(&self->video_frame);
}
