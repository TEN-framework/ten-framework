//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/stop_graph.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void ten_engine_handle_cmd_stop_graph(ten_engine_t *self, ten_shared_ptr_t *cmd,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(cmd && ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_STOP_GRAPH,
             "Should not happen.");

  const char *graph_id = ten_cmd_stop_graph_get_graph_id(cmd);
  if (graph_id == NULL || !strlen(graph_id) ||
      ten_string_is_equal_c_str(&self->graph_id, graph_id)) {
    // Suicide. Store the stop_graph command temporarily, so that it can be
    // used to return the cmd_result when the engine is shut down later.
    self->cmd_stop_graph = ten_shared_ptr_clone(cmd);

    ten_engine_close_async(self);
  } else {
    // Close other engine. Send the command to app.

    ten_app_t *app = self->app;
    TEN_ASSERT(app, "Invalid argument.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: The engine might have its own thread, and it is different
    // from the app's thread. When the engine is still alive, the app must also
    // be alive. Furthermore, the app associated with the engine remains
    // unchanged throughout the engine's lifecycle, and the app fields accessed
    // underneath are constant once the app is initialized. Therefore, the use
    // of the app here is considered thread-safe.
    TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid use of app %p.",
               app);

    ten_app_push_to_in_msgs_queue(app, cmd);
  }
}
