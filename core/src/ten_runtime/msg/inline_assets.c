//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/msg.h"

extern inline bool ten_raw_msg_is_cmd_and_result(ten_msg_t *self);  // NOLINT

extern inline bool ten_raw_msg_is_cmd(ten_msg_t *self);  // NOLINT

extern inline bool ten_raw_msg_is_cmd_result(ten_msg_t *self);  // NOLINT

extern inline ten_msg_t *ten_msg_get_raw_msg(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd_and_result(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd_result(ten_shared_ptr_t *self);  // NOLINT

extern inline TEN_MSG_TYPE ten_raw_msg_get_type(ten_msg_t *self);  // NOLINT

extern inline void ten_raw_msg_set_timestamp(ten_msg_t *self,  // NOLINT
                                             int64_t timestamp);

extern inline int64_t ten_raw_msg_get_timestamp(ten_msg_t *self);  // NOLINT

extern inline void ten_msg_set_timestamp(ten_shared_ptr_t *self,  // NOLINT
                                         int64_t timestamp);

extern inline int64_t ten_msg_get_timestamp(ten_shared_ptr_t *self);  // NOLINT
