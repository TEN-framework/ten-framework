//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/hash_table.h"

typedef struct ten_extension_t ten_extension_t;

// key: string, value: ten_msg_dest_info_t
typedef struct ten_all_msg_type_dest_info_t {
  ten_hashtable_t cmd;
  ten_hashtable_t data;
  ten_hashtable_t audio_frame;
  ten_hashtable_t video_frame;
} ten_all_msg_type_dest_info_t;

TEN_RUNTIME_PRIVATE_API void ten_all_msg_type_dest_info_init(
    ten_all_msg_type_dest_info_t *self);

TEN_RUNTIME_PRIVATE_API void ten_all_msg_type_dest_info_deinit(
    ten_all_msg_type_dest_info_t *self);
