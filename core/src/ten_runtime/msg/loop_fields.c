//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/loop_fields.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg_info.h"
#include "ten_utils/macro/check.h"

void ten_msg_field_process_data_init(ten_msg_field_process_data_t *self,
                                     const char *field_name,
                                     ten_value_t *field_value,
                                     bool is_user_defined_properties) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(field_name, "Invalid argument.");
  TEN_ASSERT(field_value, "Invalid argument.");

  self->field_name = field_name;
  self->field_value = field_value;
  self->is_user_defined_properties = is_user_defined_properties;
  self->value_is_changed_after_process = false;
}

bool ten_raw_msg_loop_all_fields(ten_msg_t *self,
                                 ten_raw_msg_process_one_field_func_t cb,
                                 void *user_data, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(cb, "Invalid argument.");

  ten_raw_msg_loop_all_fields_func_t loop_all_fields =
      ten_msg_info[ten_raw_msg_get_type(self)].loop_all_fields;
  if (!loop_all_fields) {
    return false;
  }

  return loop_all_fields(self, cb, user_data, err);
}

bool ten_msg_loop_all_fields(ten_shared_ptr_t *self,
                             ten_raw_msg_process_one_field_func_t cb,
                             void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(cb, "Invalid argument.");

  return ten_raw_msg_loop_all_fields(ten_msg_get_raw_msg(self), cb, user_data,
                                     err);
}
