//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_env/internal/return.h"

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/msg_interface/common.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"

static bool ten_env_return_result_internal(
    ten_env_t *self, ten_shared_ptr_t *cmd_result, const char *cmd_id,
    const char *seq_id, ten_env_transfer_msg_result_handler_func_t handler,
    void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
             "Should not happen.");
  TEN_ASSERT(ten_msg_get_type(cmd_result) == TEN_MSG_TYPE_CMD_RESULT,
             "Should not happen.");

  if (ten_env_is_closed(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    }
    return false;
  }

  bool err_new_created = false;
  if (!err) {
    err = ten_error_create();
    err_new_created = true;
  }

  // cmd_id is very critical in the way finding.
  if (cmd_id) {
    ten_cmd_base_set_cmd_id(cmd_result, cmd_id);
  }

  // seq_id is important if the target of the 'cmd' is a client outside TEN.
  if (seq_id) {
    ten_cmd_base_set_seq_id(cmd_result, seq_id);
  }

  bool result = true;

  switch (ten_env_get_attach_to(self)) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid use of extension %p.", extension);

    result = ten_extension_dispatch_msg(
        extension, cmd_result, TEN_RESULT_RETURN_POLICY_EACH_OK_AND_ERROR, err);
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    TEN_ASSERT(extension_group &&
                   ten_extension_group_check_integrity(extension_group, true),
               "Invalid use of extension_group %p.", extension_group);

    result = ten_extension_group_dispatch_msg(extension_group, cmd_result, err);
    break;
  }

  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
               "Invalid use of engine %p.", engine);

    result = ten_engine_dispatch_msg(engine, cmd_result);
    break;
  }

  default:
    TEN_ASSERT(0, "Handle this condition.");
    break;
  }

  if (result && handler) {
    // If the method synchronously returns true, it means that the callback must
    // be called.
    //
    // We temporarily assume that the message enqueue represents success;
    // therefore, in this case, we set the error to NULL to indicate that the
    // returning was successful.
    handler(self, NULL, user_data, NULL);
  }

  if (err_new_created) {
    ten_error_destroy(err);
  }

  return result;
}

// If the 'cmd' has already been a command in the backward path, a extension
// could use this API to return the 'cmd' further.
bool ten_env_return_result(ten_env_t *self, ten_shared_ptr_t *cmd_result,
                           ten_env_transfer_msg_result_handler_func_t handler,
                           void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(cmd_result && ten_cmd_base_check_integrity(cmd_result),
             "Should not happen.");
  TEN_ASSERT(ten_msg_get_type(cmd_result) == TEN_MSG_TYPE_CMD_RESULT,
             "The target cmd must be a cmd result.");

  return ten_env_return_result_internal(self, cmd_result, NULL, NULL, handler,
                                        user_data, err);
}
