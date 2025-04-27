//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_env/internal/send.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/msg_not_connected_cnt.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/msg_interface/common.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/result_return_policy.h"
#include "include_internal/ten_runtime/ten_env/send.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

ten_env_send_cmd_options_t *ten_env_send_cmd_options_create(void) {
  ten_env_send_cmd_options_t *options =
      TEN_MALLOC(sizeof(ten_env_send_cmd_options_t));
  TEN_ASSERT(options, "Failed to allocate memory.");

  *options = TEN_ENV_SEND_CMD_OPTIONS_INIT_VAL;

  return options;
}

void ten_env_send_cmd_options_destroy(ten_env_send_cmd_options_t *options) {
  TEN_ASSERT(options, "Invalid argument.");

  TEN_FREE(options);
}

/**
 * @brief All message-sending code paths will ultimately converge in this
 * function.
 */
static bool ten_env_send_msg_internal(
    ten_env_t *self, ten_shared_ptr_t *msg,
    ten_env_transfer_msg_result_handler_func_t handler, void *user_data,
    TEN_RESULT_RETURN_POLICY result_return_policy, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  if (ten_env_is_closed(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_TEN_IS_CLOSED, "ten_env is closed.");
    }
    return false;
  }

  const bool msg_is_cmd = ten_msg_is_cmd(msg);

  // Even if the user does not pass in the `err` parameter, since different
  // error scenarios require different handling, we need to create a temporary
  // one to obtain the actual error information.
  bool err_new_created = false;
  if (!err) {
    err = ten_error_create();
    err_new_created = true;
  }

  bool result = true;

  if (!msg) {
    ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, "Msg is empty.");
    result = false;
    goto done;
  }

  if (ten_msg_get_type(msg) == TEN_MSG_TYPE_CMD_RESULT) {
    // The only way to send out the 'status' command from extension is through
    // various 'return_xxx' functions.
    TEN_LOGE("Result commands should be delivered through 'returning'.");
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "Result commands should be delivered through 'returning'.");
    result = false;
    goto done;
  }

  if (ten_msg_has_locked_res(msg)) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "Locked resources are not allowed in messages sent from an "
                  "extension.");
    result = false;
    goto done;
  }

  if (msg_is_cmd) {
    // 'command' has the mechanism of 'result'.
    ten_cmd_base_set_result_handler(msg, handler, user_data);

    // @{
    // All commands sent from an extension will eventually go to this function,
    // and command ID plays a critical role in the TEN runtime, therefore, when
    // a command is sent from an extension, and doesn't contain a command ID,
    // TEN runtime must assign it a command ID _before_ sending it to the TEN
    // runtime deeper.
    const char *cmd_id = ten_cmd_base_get_cmd_id(msg);
    if (!strlen(cmd_id)) {
      cmd_id = ten_cmd_base_gen_new_cmd_id_forcibly(msg);
    }
    // @}
  }

  switch (ten_env_get_attach_to(self)) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    TEN_ASSERT(extension, "Should not happen.");

    result =
        ten_extension_dispatch_msg(extension, msg, result_return_policy, err);
    break;
  }

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    TEN_ASSERT(extension_group, "Should not happen.");

    result = ten_extension_group_dispatch_msg(extension_group, msg, err);
    break;
  }

  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    TEN_ASSERT(engine, "Should not happen.");

    result = ten_engine_dispatch_msg(engine, msg);
    break;
  }

  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    TEN_ASSERT(app, "Should not happen.");

    result = ten_app_dispatch_msg(app, msg, err);
    break;
  }

  default:
    TEN_ASSERT(0, "Handle more conditions: %d", ten_env_get_attach_to(self));
    break;
  }

done:
  if (!result) {
    if (ten_error_code(err) == TEN_ERROR_CODE_MSG_NOT_CONNECTED) {
      if (ten_env_get_attach_to(self) == TEN_ENV_ATTACH_TO_EXTENSION) {
        ten_extension_t *extension = ten_env_get_attached_extension(self);
        TEN_ASSERT(extension, "Should not happen.");

        if (ten_extension_increment_msg_not_connected_count(
                extension, ten_msg_get_name(msg))) {
          TEN_LOGW("Failed to send message: %s", ten_error_message(err));
        }
      } else {
        TEN_LOGE("Failed to send message: %s", ten_error_message(err));
      }
    } else {
      TEN_LOGE("Failed to send message: %s", ten_error_message(err));
    }
  }

  if (err_new_created) {
    ten_error_destroy(err);
  }

  if (result && handler && !msg_is_cmd) {
    // If the method synchronously returns true, it means that the callback must
    // be called.
    //
    // For command-type messages, its result handler will be invoked when the
    // target extension returns a response.
    //
    // TODO(Wei): For other types of messages, such as
    // data/audio_frame/video_frame, we temporarily consider the sending to be
    // successful right after the message is enqueued (In the future, this time
    // point might change, but overall, as long as the result handler is
    // provided, it must be invoked in this case). Therefore, it is necessary to
    // execute the callback function and set the error to NULL to indicate that
    // no error has occurred.
    handler(self, NULL, user_data, NULL);
  }

  return result;
}

bool ten_env_send_cmd(ten_env_t *self, ten_shared_ptr_t *cmd,
                      ten_env_transfer_msg_result_handler_func_t handler,
                      void *user_data, ten_env_send_cmd_options_t *options,
                      ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(cmd, "Should not happen.");

  bool rc = false;

  if (handler) {
    if (!options || options->enable_multiple_results == false) {
      // The TEN runtime will only pass the final result up to the upper layer,
      // and the upper layer can expect to receive only one result.
      rc = ten_env_send_msg_internal(
          self, cmd, handler, user_data,
          TEN_RESULT_RETURN_POLICY_FIRST_ERROR_OR_LAST_OK, err);
    } else {
      // The TEN runtime will pass all received results up to the upper layer,
      // where they will be handled.
      rc = ten_env_send_msg_internal(self, cmd, handler, user_data,
                                     TEN_RESULT_RETURN_POLICY_EACH_OK_AND_ERROR,
                                     err);
    }
  } else {
    TEN_ASSERT(!user_data, "Should not happen.");

    rc = ten_env_send_msg_internal(
        self, cmd, NULL, NULL, TEN_RESULT_RETURN_POLICY_FIRST_ERROR_OR_LAST_OK,
        err);
  }

  return rc;
}

bool ten_env_send_data(ten_env_t *self, ten_shared_ptr_t *data,
                       ten_env_transfer_msg_result_handler_func_t handler,
                       void *user_data, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(data, "Should not happen.");

  return ten_env_send_msg_internal(self, data, handler, user_data,
                                   TEN_RESULT_RETURN_POLICY_INVALID, err);
}

bool ten_env_send_video_frame(
    ten_env_t *self, ten_shared_ptr_t *frame,
    ten_env_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(frame, "Should not happen.");

  return ten_env_send_msg_internal(self, frame, handler, user_data,
                                   TEN_RESULT_RETURN_POLICY_INVALID, err);
}

bool ten_env_send_audio_frame(
    ten_env_t *self, ten_shared_ptr_t *frame,
    ten_env_transfer_msg_result_handler_func_t handler, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(frame, "Should not happen.");

  return ten_env_send_msg_internal(self, frame, handler, user_data,
                                   TEN_RESULT_RETURN_POLICY_INVALID, err);
}
