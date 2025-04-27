//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion_context.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg_conversion/msg_and_its_result_conversion.h"
#include "include_internal/ten_runtime/msg_conversion/msg_and_result_conversion.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/base.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"

bool ten_msg_conversion_context_check_integrity(
    ten_msg_conversion_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_MSG_CONVERSIONS_SIGNATURE) {
    return false;
  }
  return true;
}

ten_msg_conversion_context_t *ten_msg_conversion_context_create(
    const char *msg_name) {
  TEN_ASSERT(msg_name, "Invalid argument.");

  ten_msg_conversion_context_t *self =
      (ten_msg_conversion_context_t *)TEN_MALLOC(
          sizeof(ten_msg_conversion_context_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_MSG_CONVERSIONS_SIGNATURE);

  ten_string_init_formatted(&self->msg_name, "%s", msg_name);
  self->msg_and_result_conversion = NULL;

  return self;
}

void ten_msg_conversion_context_destroy(ten_msg_conversion_context_t *self) {
  TEN_ASSERT(self && ten_msg_conversion_context_check_integrity(self),
             "Should not happen.");

  ten_string_deinit(&self->msg_name);
  ten_loc_deinit(&self->src_loc);
  if (self->msg_and_result_conversion) {
    ten_msg_and_result_conversion_destroy(self->msg_and_result_conversion);
  }

  TEN_FREE(self);
}

static bool ten_msg_conversion_is_equal(ten_msg_conversion_context_t *a,
                                        ten_msg_conversion_context_t *b) {
  TEN_ASSERT(a && ten_msg_conversion_context_check_integrity(a),
             "Should not happen.");
  TEN_ASSERT(b && ten_msg_conversion_context_check_integrity(b),
             "Should not happen.");

  if (!ten_loc_is_equal(&a->src_loc, &b->src_loc)) {
    return false;
  }

  if (!ten_string_is_equal(&a->msg_name, &b->msg_name)) {
    return false;
  }

  return true;
}

static bool ten_msg_conversion_can_match_msg(ten_msg_conversion_context_t *self,
                                             ten_shared_ptr_t *msg) {
  TEN_ASSERT(self && ten_msg_conversion_context_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  const char *name = ten_msg_get_name(msg);
  TEN_ASSERT(name, "Should not happen.");

  // @{
  // Because when we parse the graph declaration (ex: from JSON), we don't
  // know the graph ID at that time, so the graph ID in 'msg_conversions'
  // will be empty, so we need to update the correct graph ID here, so
  // that the following 'ten_loc_is_equal' could success.
  if (ten_string_is_empty(&self->src_loc.graph_id)) {
    ten_string_copy(&self->src_loc.graph_id,
                    &ten_msg_get_src_loc(msg)->graph_id);
  }
  // @}

  if (!ten_loc_is_equal(ten_msg_get_src_loc(msg), &self->src_loc) ||
      !ten_string_is_equal_c_str(&self->msg_name, name)) {
    return false;
  }

  return true;
}

bool ten_msg_conversion_context_merge(
    ten_list_t *msg_conversions,
    ten_msg_conversion_context_t *new_msg_conversion, ten_error_t *err) {
  TEN_ASSERT(msg_conversions && ten_list_check_integrity(msg_conversions),
             "Should not happen.");
  TEN_ASSERT(new_msg_conversion &&
                 ten_msg_conversion_context_check_integrity(new_msg_conversion),
             "Should not happen.");

  ten_list_foreach (msg_conversions, iter) {
    ten_msg_conversion_context_t *msg_conversion =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(msg_conversion &&
                   ten_msg_conversion_context_check_integrity(msg_conversion),
               "Should not happen.");

    if (ten_msg_conversion_is_equal(msg_conversion, new_msg_conversion)) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_INVALID_GRAPH,
                      "Duplicated message conversion.");
      }
      ten_msg_conversion_context_destroy(new_msg_conversion);
      return false;
    }
  }

  ten_list_push_ptr_back(
      msg_conversions, new_msg_conversion,
      (ten_ptr_listnode_destroy_func_t)ten_msg_conversion_context_destroy);

  return true;
}

bool ten_extension_convert_msg(ten_extension_t *self, ten_shared_ptr_t *msg,
                               ten_list_t *result, ten_error_t *err) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Invalid argument");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");
  TEN_ASSERT(result, "Should not happen.");
  TEN_ASSERT(err && ten_error_check_integrity(err), "Invalid argument.");

  bool something_wrong = false;

  if (ten_msg_is_cmd_and_result(msg) &&
      ten_msg_get_type(msg) != TEN_MSG_TYPE_CMD) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "Can not convert a builtin cmd.");
    something_wrong = true;
    goto done;
  }

  TEN_ASSERT(
      ten_list_check_integrity(&self->extension_info->msg_conversion_contexts),
      "Should not happen.");

  ten_list_foreach (&self->extension_info->msg_conversion_contexts, iter) {
    ten_msg_conversion_context_t *msg_conversions =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(msg_conversions &&
                   ten_msg_conversion_context_check_integrity(msg_conversions),
               "Should not happen.");

    // Find the correct message conversion function according to the current
    // source extension and the key of the message.
    if (ten_msg_conversion_can_match_msg(msg_conversions, msg)) {
      if (msg_conversions->msg_and_result_conversion) {
        ten_msg_conversion_t *msg_conversion =
            msg_conversions->msg_and_result_conversion->msg;
        TEN_ASSERT(msg_conversion, "Invalid argument.");

        // Perform the message conversions.
        ten_shared_ptr_t *new_msg =
            ten_msg_conversion_convert(msg_conversion, msg, err);

        if (new_msg) {
          // Note: Although there might multiple messages been
          // converted/generated at once, and for a command, the command IDs of
          // those converted commands are equal, we do _not_ need to consider to
          // change the command IDs of those converted commands to different
          // values. Because those converted commands will be transmitted to an
          // extension, and just before entering that extension, TEN runtime
          // will add corresponding IN path into the IN path table, and at that
          // time, TEN runtime will detect there has already been a IN path in
          // the IN path table with the same command ID, and change the command
          // ID of the currently processed command to a different value at that
          // time.

          ten_list_push_ptr_back(
              result,
              ten_msg_and_its_result_conversion_create(
                  new_msg, msg_conversions->msg_and_result_conversion->result),
              (ten_ptr_listnode_destroy_func_t)
                  ten_msg_and_its_result_conversion_destroy);

          ten_shared_ptr_destroy(new_msg);
        } else {
          something_wrong = true;
        }
      }
    }
  }

  // If there is no matched message conversions, put the original message to
  // 'result'.
  if (ten_list_is_empty(result)) {
    ten_list_push_ptr_back(result,
                           ten_msg_and_its_result_conversion_create(msg, NULL),
                           (ten_ptr_listnode_destroy_func_t)
                               ten_msg_and_its_result_conversion_destroy);
  }

done:
  return !something_wrong;
}

static ten_msg_conversion_context_t *ten_msg_conversion_from_json_internal(
    ten_json_t *json, ten_loc_t *src_loc, const char *original_cmd_name,
    ten_error_t *err) {
  TEN_ASSERT(json, "Invalid argument.");
  TEN_ASSERT(original_cmd_name, "Invalid argument.");

  ten_msg_conversion_context_t *self =
      ten_msg_conversion_context_create(original_cmd_name);

  ten_loc_init_from_loc(&self->src_loc, src_loc);

  TEN_ASSERT(ten_json_is_object(json), "Should not happen.");

  ten_msg_and_result_conversion_t *msg_result_operation =
      ten_msg_and_result_conversion_from_json(json, err);
  TEN_ASSERT(msg_result_operation, "Should not happen.");
  if (!msg_result_operation) {
    ten_msg_conversion_context_destroy(self);
    return NULL;
  }

  self->msg_and_result_conversion = msg_result_operation;

  return self;
}

ten_msg_conversion_context_t *ten_msg_conversion_context_from_json(
    ten_json_t *json, ten_extension_info_t *src_extension_info,
    const char *cmd_name, ten_error_t *err) {
  TEN_ASSERT(json && src_extension_info, "Should not happen.");

  return ten_msg_conversion_from_json_internal(json, &src_extension_info->loc,
                                               cmd_name, err);
}

static ten_msg_conversion_context_t *ten_msg_conversion_from_value_internal(
    ten_value_t *value, ten_loc_t *src_loc, const char *cmd_name,
    ten_error_t *err) {
  TEN_ASSERT(value && cmd_name, "Should not happen.");

  ten_msg_conversion_context_t *self =
      ten_msg_conversion_context_create(cmd_name);

  ten_loc_init_from_loc(&self->src_loc, src_loc);

  TEN_ASSERT(value->type == TEN_TYPE_OBJECT, "Should not happen.");

  ten_msg_and_result_conversion_t *msg_result_operation =
      ten_msg_and_result_conversion_from_value(value, err);
  TEN_ASSERT(msg_result_operation, "Should not happen.");
  if (!msg_result_operation) {
    ten_msg_conversion_context_destroy(self);
    return NULL;
  }

  self->msg_and_result_conversion = msg_result_operation;

  return self;
}

ten_msg_conversion_context_t *ten_msg_conversion_context_from_value(
    ten_value_t *value, ten_extension_info_t *src_extension_info,
    const char *cmd_name, ten_error_t *err) {
  TEN_ASSERT(value && src_extension_info, "Should not happen.");

  return ten_msg_conversion_from_value_internal(value, &src_extension_info->loc,
                                                cmd_name, err);
}
