//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value_get.h"

void ten_raw_msg_name_copy(ten_msg_t *self, ten_msg_t *src,
                           TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_msg_check_integrity(src), "Should not happen.");
  ten_string_copy(ten_value_peek_string(&self->name),
                  ten_value_peek_string(&src->name));
}

bool ten_raw_msg_name_process(ten_msg_t *self,
                              ten_raw_msg_process_one_field_func_t cb,
                              void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_msg_field_process_data_t name_field;
  ten_msg_field_process_data_init(&name_field, TEN_STR_NAME, &self->name,
                                  false);

  bool rc = cb(self, &name_field, user_data, err);

  if (name_field.value_is_changed_after_process) {
    TEN_MSG_TYPE msg_type_spec_by_name = ten_msg_type_from_unique_name_string(
        ten_value_peek_raw_str(name_field.field_value, err));
    if (msg_type_spec_by_name != TEN_MSG_TYPE_INVALID) {
      self->type = msg_type_spec_by_name;
    }
  }

  return rc;
}
