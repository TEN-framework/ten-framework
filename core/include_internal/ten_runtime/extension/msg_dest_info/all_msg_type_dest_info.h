//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"

typedef struct ten_extension_t ten_extension_t;

typedef struct ten_all_msg_type_dest_info_t {
  ten_list_t cmd;          // ten_shared_ptr_t of ten_msg_dest_info_t
  ten_list_t data;         // ten_shared_ptr_t of ten_msg_dest_info_t
  ten_list_t video_frame;  // ten_shared_ptr_t of ten_msg_dest_info_t
  ten_list_t audio_frame;  // ten_shared_ptr_t of ten_msg_dest_info_t
} ten_all_msg_type_dest_info_t;

TEN_RUNTIME_PRIVATE_API void ten_all_msg_type_dest_info_init(
    ten_all_msg_type_dest_info_t *self);

TEN_RUNTIME_PRIVATE_API void ten_all_msg_type_dest_info_deinit(
    ten_all_msg_type_dest_info_t *self);
