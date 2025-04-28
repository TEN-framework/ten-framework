//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/cmd_base/cmd/cmd.h"

#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/custom/cmd.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/custom/field/field_info.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"

static void ten_raw_cmd_custom_destroy(ten_cmd_t *self) {
  TEN_ASSERT(
      self && ten_raw_msg_get_type((ten_msg_t *)self) == TEN_MSG_TYPE_CMD,
      "Should not happen.");

  ten_raw_cmd_deinit(self);
  TEN_FREE(self);
}

void ten_raw_cmd_custom_as_msg_destroy(ten_msg_t *self) {
  TEN_ASSERT(self && ten_raw_msg_get_type(self) == TEN_MSG_TYPE_CMD,
             "Should not happen.");

  ten_raw_cmd_custom_destroy((ten_cmd_t *)self);
}

static ten_cmd_t *ten_raw_cmd_custom_create_empty(void) {
  ten_cmd_t *raw_cmd = TEN_MALLOC(sizeof(ten_cmd_t));
  TEN_ASSERT(raw_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(raw_cmd, TEN_MSG_TYPE_CMD);

  return raw_cmd;
}

ten_shared_ptr_t *ten_cmd_custom_create_empty(void) {
  return ten_shared_ptr_create(ten_raw_cmd_custom_create_empty(),
                               ten_raw_cmd_custom_destroy);
}

ten_cmd_t *ten_raw_cmd_custom_create(const char *name, ten_error_t *err) {
  TEN_ASSERT(name, "Should not happen.");

  ten_cmd_t *cmd = ten_raw_cmd_custom_create_empty();
  TEN_ASSERT(cmd && ten_raw_cmd_check_integrity(cmd), "Should not happen.");

  ten_raw_msg_set_name((ten_msg_t *)cmd, name, err);

  return cmd;
}

static ten_cmd_t *ten_raw_cmd_custom_create_with_name_len(const char *name,
                                                          size_t name_len,
                                                          ten_error_t *err) {
  TEN_ASSERT(name, "Should not happen.");

  ten_cmd_t *cmd = ten_raw_cmd_custom_create_empty();
  TEN_ASSERT(cmd && ten_raw_cmd_check_integrity(cmd), "Should not happen.");

  ten_raw_msg_set_name_with_len((ten_msg_t *)cmd, name, name_len, err);

  return cmd;
}

ten_shared_ptr_t *ten_cmd_custom_create(const char *name, ten_error_t *err) {
  TEN_ASSERT(name, "Should not happen.");
  return ten_shared_ptr_create(ten_raw_cmd_custom_create(name, err),
                               ten_raw_cmd_custom_destroy);
}

ten_shared_ptr_t *ten_cmd_custom_create_with_name_len(const char *name,
                                                      size_t name_len,
                                                      ten_error_t *err) {
  TEN_ASSERT(name, "Should not happen.");
  return ten_shared_ptr_create(
      ten_raw_cmd_custom_create_with_name_len(name, name_len, err),
      ten_raw_cmd_custom_destroy);
}

ten_msg_t *ten_raw_cmd_custom_as_msg_clone(ten_msg_t *self,
                                           ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && ten_raw_cmd_base_check_integrity((ten_cmd_base_t *)self) &&
                 ten_raw_msg_get_type(self) == TEN_MSG_TYPE_CMD,
             "Should not happen.");

  ten_cmd_t *new_cmd = TEN_MALLOC(sizeof(ten_cmd_t));
  TEN_ASSERT(new_cmd, "Failed to allocate memory.");

  ten_raw_cmd_init(new_cmd, TEN_MSG_TYPE_CMD);

  for (size_t i = 0; i < ten_cmd_custom_fields_info_size; ++i) {
    if (excluded_field_ids) {
      bool skip = false;

      ten_list_foreach (excluded_field_ids, iter) {
        if (ten_cmd_custom_fields_info[i].field_id ==
            ten_int32_listnode_get(iter.node)) {
          skip = true;
          break;
        }
      }

      if (skip) {
        continue;
      }
    }

    ten_msg_copy_field_func_t copy_field =
        ten_cmd_custom_fields_info[i].copy_field;
    if (copy_field) {
      copy_field((ten_msg_t *)new_cmd, self, excluded_field_ids);
    }
  }

  return (ten_msg_t *)new_cmd;
}

bool ten_raw_cmd_custom_set_ten_property(ten_msg_t *self, ten_list_t *paths,
                                         ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(paths && ten_list_check_integrity(paths),
             "path should not be empty.");
  TEN_ASSERT(value && ten_value_check_integrity(value), "Invalid argument.");

  bool success = true;

  ten_error_t tmp_err;
  bool use_tmp_err = false;
  if (!err) {
    use_tmp_err = true;
    ten_error_init(&tmp_err);
    err = &tmp_err;
  }

  ten_list_foreach (paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if (!strcmp(TEN_STR_NAME, ten_string_get_raw_str(&item->obj_item_str))) {
        if (ten_value_is_string(value)) {
          ten_value_set_string_with_size(
              &self->name, ten_value_peek_raw_str(value, &tmp_err),
              strlen(ten_value_peek_raw_str(value, &tmp_err)));
          success = true;
        } else {
          success = false;
        }
      }
      break;
    }

    default:
      break;
    }
  }

  if (use_tmp_err) {
    ten_error_deinit(&tmp_err);
  }

  return success;
}

bool ten_raw_cmd_custom_loop_all_fields(ten_msg_t *self,
                                        ten_raw_msg_process_one_field_func_t cb,
                                        void *user_data, ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_cmd_check_integrity((ten_cmd_t *)self) && cb,
             "Should not happen.");

  for (size_t i = 0; i < ten_cmd_custom_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_cmd_custom_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
