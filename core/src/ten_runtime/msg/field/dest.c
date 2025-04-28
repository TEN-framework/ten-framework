//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/field/dest.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/msg/loop_fields.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"

void ten_raw_msg_dest_copy(ten_msg_t *self, ten_msg_t *src,
                           TEN_UNUSED ten_list_t *excluded_field_ids) {
  TEN_ASSERT(src && ten_raw_msg_check_integrity(src), "Should not happen.");

  ten_list_clear(&self->dest_loc);
  ten_list_foreach (&src->dest_loc, iter) {
    ten_loc_t *dest_loc = ten_ptr_listnode_get(iter.node);
    ten_loc_t *cloned_dest = ten_loc_clone(dest_loc);
    ten_list_push_ptr_back(&self->dest_loc, cloned_dest,
                           (ten_ptr_listnode_destroy_func_t)ten_loc_destroy);
  }
}

bool ten_raw_msg_dest_process(ten_msg_t *self,
                              ten_raw_msg_process_one_field_func_t cb,
                              void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  ten_list_t dest_value_list = TEN_LIST_INIT_VAL;
  ten_list_foreach (&self->dest_loc, iter) {
    ten_loc_t *loc = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(loc, "Should not happen.");

    ten_value_t *loc_value = ten_loc_to_value(loc);
    TEN_ASSERT(loc_value, "Should not happen.");

    ten_list_push_ptr_back(&dest_value_list, loc_value,
                           (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
  }

  ten_value_t *dest_value = ten_value_create_array_with_move(&dest_value_list);
  TEN_ASSERT(dest_value, "Should not happen.");

  ten_msg_field_process_data_t dest_field;
  ten_msg_field_process_data_init(&dest_field, TEN_STR_DEST, dest_value, false);

  bool rc = cb(self, &dest_field, user_data, err);

  if (dest_field.value_is_changed_after_process) {
    ten_list_clear(&self->dest_loc);

    ten_value_array_foreach(dest_field.field_value, iter) {
      ten_value_t *loc_value = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(loc_value, "Should not happen.");

      ten_list_push_ptr_back(&self->dest_loc,
                             ten_loc_create_from_value(loc_value),
                             (ten_ptr_listnode_destroy_func_t)ten_loc_destroy);
    }
  }

  ten_value_destroy(dest_value);
  ten_list_clear(&dest_value_list);

  return rc;
}
