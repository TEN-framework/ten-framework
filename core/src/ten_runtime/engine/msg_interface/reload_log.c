//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/msg_interface/reload_log.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_engine_handle_cmd_reload_log(ten_engine_t *self, ten_shared_ptr_t *cmd,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd && ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_RELOAD_LOG,
             "Should not happen.");

  ten_app_t *app = self->app;
  TEN_ASSERT(app, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The app outlives its engines and remains unchanged while an
  // engine is alive. The queue operation itself is thread-safe.
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid use of app %p.",
             app);

  ten_app_push_to_in_msgs_queue(app, cmd);
}
