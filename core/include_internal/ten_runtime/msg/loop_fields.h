//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/value/value_kv.h"

typedef struct ten_msg_t ten_msg_t;

typedef struct ten_msg_field_process_data_t {
  // The name of the field.
  const char *field_name;

  // The value of the field.
  ten_value_t *field_value;

  // Whether this field is a user-defined field. If it is not user-defined, then
  // it is a `ten` field.
  bool is_user_defined_properties;

  // Whether the value has been modified. Some logic checks if the value has
  // changed, requiring it to be written back to the original memory space for
  // that field. For example, in the message, `src/dest loc` exists as a
  // `ten_loc_t` type. If, within certain process logic, the value is modified,
  // the updated value should reflect in the corresponding `ten_loc_t`. The
  // `value_is_changed_after_process` field indicates whether the value has been
  // modified, allowing users to determine if it should be written back to the
  // original field.
  bool value_is_changed_after_process;
} ten_msg_field_process_data_t;

typedef bool (*ten_raw_msg_process_one_field_func_t)(
    ten_msg_t *msg, ten_msg_field_process_data_t *field, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_API void ten_msg_field_process_data_init(
    ten_msg_field_process_data_t *self, const char *field_name,
    ten_value_t *field_value, bool is_user_defined_properties);

TEN_RUNTIME_API bool ten_raw_msg_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err);

TEN_RUNTIME_API bool ten_msg_loop_all_fields(
    ten_shared_ptr_t *self, ten_raw_msg_process_one_field_func_t cb,
    void *user_data, ten_error_t *err);
