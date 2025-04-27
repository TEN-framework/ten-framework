//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_dest_info/value.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/extension_info/value.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "include_internal/ten_runtime/msg_conversion/msg_and_result_conversion.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion_context.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

ten_value_t *ten_msg_dest_info_to_value(
    ten_msg_dest_info_t *self, ten_extension_info_t *src_extension_info,
    ten_error_t *err) {
  TEN_ASSERT(self && ten_msg_dest_info_check_integrity(self),
             "Should not happen.");

  ten_list_t value_object_kv_list = TEN_LIST_INIT_VAL;

  ten_list_push_ptr_back(
      &value_object_kv_list,
      ten_value_kv_create(
          TEN_STR_NAME,
          ten_value_create_string(ten_string_get_raw_str(&self->name))),
      (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

  ten_list_t dests_list = TEN_LIST_INIT_VAL;

  ten_list_foreach (&self->dest, iter) {
    ten_weak_ptr_t *dest = ten_smart_ptr_listnode_get(iter.node);
    TEN_ASSERT(dest, "Invalid argument.");

    ten_extension_info_t *extension_info = ten_smart_ptr_get_data(dest);
    TEN_ASSERT(extension_info, "Should not happen.");

    ten_list_t dest_kv_list = TEN_LIST_INIT_VAL;

    ten_list_push_ptr_back(
        &dest_kv_list,
        ten_value_kv_create(TEN_STR_APP,
                            ten_value_create_string(ten_string_get_raw_str(
                                &extension_info->loc.app_uri))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

    ten_list_push_ptr_back(
        &dest_kv_list,
        ten_value_kv_create(TEN_STR_GRAPH,
                            ten_value_create_string(ten_string_get_raw_str(
                                &extension_info->loc.graph_id))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

    ten_list_push_ptr_back(
        &dest_kv_list,
        ten_value_kv_create(TEN_STR_EXTENSION_GROUP,
                            ten_value_create_string(ten_string_get_raw_str(
                                &extension_info->loc.extension_group_name))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

    ten_list_push_ptr_back(
        &dest_kv_list,
        ten_value_kv_create(TEN_STR_EXTENSION,
                            ten_value_create_string(ten_string_get_raw_str(
                                &extension_info->loc.extension_name))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);

    bool found = false;

    ten_list_foreach (&extension_info->msg_conversion_contexts,
                      msg_conversion_iter) {
      ten_msg_conversion_context_t *msg_conversion_context =
          ten_ptr_listnode_get(msg_conversion_iter.node);
      TEN_ASSERT(
          msg_conversion_context && ten_msg_conversion_context_check_integrity(
                                        msg_conversion_context),
          "Should not happen.");

      if (ten_loc_is_equal(&src_extension_info->loc,
                           &msg_conversion_context->src_loc) &&
          ten_string_is_equal(&msg_conversion_context->msg_name, &self->name)) {
        TEN_ASSERT(found == false, "Should not happen.");
        found = true;

        ten_value_t *msg_and_result_conversion_operation_value =
            ten_msg_and_result_conversion_to_value(
                msg_conversion_context->msg_and_result_conversion, err);

        if (!msg_and_result_conversion_operation_value) {
          TEN_LOGE("Failed to convert msg_and_result_conversion_operation.");
          continue;
        }

        ten_list_push_ptr_back(
            &dest_kv_list,
            ten_value_kv_create(TEN_STR_MSG_CONVERSION,
                                msg_and_result_conversion_operation_value),
            (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
      }
    }

    ten_value_t *dest_value = ten_value_create_object_with_move(&dest_kv_list);
    ten_list_push_ptr_back(&dests_list, dest_value,
                           (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
    ten_list_clear(&dest_kv_list);
  }

  ten_value_t *dests_value = ten_value_create_array_with_move(&dests_list);
  ten_list_push_ptr_back(&value_object_kv_list,
                         ten_value_kv_create(TEN_STR_DEST, dests_value),
                         (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  ten_value_t *value = ten_value_create_object_with_move(&value_object_kv_list);
  ten_list_clear(&value_object_kv_list);

  return value;
}

// Parse the following snippet.
//
// ------------------------
// "name": "...",
// "dest": [{
//   "app": "...",
//   "extension_group": "...",
//   "extension": "...",
//   "msg_conversion": {
//   }
// }]
// ------------------------
ten_shared_ptr_t *ten_msg_dest_info_from_value(
    ten_value_t *value, ten_list_t *extensions_info,
    ten_extension_info_t *src_extension_info, ten_error_t *err) {
  TEN_ASSERT(value && extensions_info, "Should not happen.");
  TEN_ASSERT(src_extension_info,
             "src_extension must be specified in this case.");

  ten_msg_dest_info_t *self = NULL;

  // "name": "...",
  ten_value_t *name_value = ten_value_object_peek(value, TEN_STR_NAME);

  const char *name = "";
  if (name_value) {
    name = ten_value_peek_raw_str(name_value, err);
  }

  self = ten_msg_dest_info_create(name);
  TEN_ASSERT(self, "Should not happen.");

  // "dest": [{
  //   "app": "...",
  //   "extension_group": "...",
  //   "extension": "...",
  //   "msg_conversion": {
  //   }
  // }]
  ten_value_t *dests_value = ten_value_object_peek(value, TEN_STR_DEST);
  if (dests_value) {
    if (!ten_value_is_array(dests_value)) {
      goto error;
    }

    ten_value_array_foreach(dests_value, iter) {
      ten_value_t *dest_value = ten_ptr_listnode_get(iter.node);
      if (!ten_value_is_object(dest_value)) {
        goto error;
      }

      ten_shared_ptr_t *dest =
          ten_extension_info_parse_connection_dest_part_from_value(
              dest_value, extensions_info, src_extension_info, name, err);
      if (!dest) {
        goto error;
      }

      // We need to use weak_ptr here to prevent the circular shared_ptr problem
      // in the case of loop graph.
      ten_weak_ptr_t *weak_dest = ten_weak_ptr_create(dest);
      ten_list_push_smart_ptr_back(&self->dest, weak_dest);
      ten_weak_ptr_destroy(weak_dest);
    }
  }

  goto done;

error:
  if (self) {
    ten_msg_dest_info_destroy(self);
    self = NULL;
  }

done:
  if (self) {
    return ten_shared_ptr_create(self, ten_msg_dest_info_destroy);
  } else {
    return NULL;
  }
}
