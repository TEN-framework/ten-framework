//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_dest_info/all_msg_type_dest_info.h"

#include "ten_utils/macro/check.h"

void ten_all_msg_type_dest_info_init(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_list_init(&self->cmd);
  ten_list_init(&self->video_frame);
  ten_list_init(&self->audio_frame);
  ten_list_init(&self->data);
}

void ten_all_msg_type_dest_info_deinit(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_list_clear(&self->cmd);
  ten_list_clear(&self->video_frame);
  ten_list_clear(&self->audio_frame);
  ten_list_clear(&self->data);
}
