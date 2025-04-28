//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/field/src.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

void ten_raw_msg_src_copy(ten_msg_t *self, ten_msg_t *src,
                          ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_msg_check_integrity(src), "Should not happen.");
  ten_loc_set_from_loc(&self->src_loc, &src->src_loc);
}

bool ten_raw_msg_src_process(ten_msg_t *self,
                             ten_raw_msg_process_one_field_func_t cb,
                             void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_value_t *src_value = ten_loc_to_value(&self->src_loc);
  TEN_ASSERT(ten_value_is_object(src_value), "Should not happen.");

  ten_msg_field_process_data_t src_field;
  ten_msg_field_process_data_init(&src_field, TEN_STR_SRC, src_value, false);

  bool rc = cb(self, &src_field, user_data, err);

  if (src_field.value_is_changed_after_process) {
    ten_loc_set_from_value(&self->src_loc, src_field.field_value);
  }

  ten_value_destroy(src_value);
  return rc;
}
