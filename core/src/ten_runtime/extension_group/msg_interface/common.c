//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/msg_interface/common.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/msg/msg.h"

bool ten_extension_group_dispatch_msg(ten_extension_group_t *self,
                                      ten_shared_ptr_t *msg, ten_error_t *err) {
  ten_msg_set_src_to_extension_group(msg, self);

  ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(msg);
  TEN_ASSERT(dest_loc, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(dest_loc), "Should not happen.");
  TEN_ASSERT(ten_msg_get_dest_cnt(msg) == 1, "Should not happen.");
  TEN_ASSERT(!ten_string_is_empty(&dest_loc->app_uri),
             "App URI should not be empty.");

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context, "Invalid argument.");
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, false),
             "Invalid argument.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Invalid argument.");

  ten_app_t *app = engine->app;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid argument.");

  if (!ten_string_is_equal_c_str(&dest_loc->app_uri, ten_app_get_uri(app))) {
    // Send to other apps.
    TEN_ASSERT(0, "Handle this condition.");
  } else {
    if (ten_string_is_empty(&dest_loc->graph_id)) {
      // It means asking the app to do something.
      TEN_ASSERT(0, "Handle this condition.");
    } else if (ten_string_is_equal_c_str(&dest_loc->graph_id,
                                         ten_engine_get_id(engine, false))) {
      if (ten_string_is_empty(&dest_loc->extension_group_name)) {
        // It means asking the engine to do something.
        ten_engine_append_to_in_msgs_queue(engine, msg);
      } else {
        // Send to other extension group in the same engine.
        TEN_ASSERT(0, "Handle this condition.");
      }
    } else {
      // Send to other engines in the same app. The message should not be
      // handled in this engine, so ask the app to handle this message.
      ten_app_push_to_in_msgs_queue(app, msg);
    }
  }

  return true;
}
