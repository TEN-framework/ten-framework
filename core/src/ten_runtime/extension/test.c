//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/msg/msg.h"

void ten_adjust_msg_dest_for_standalone_test_scenario(
    ten_shared_ptr_t *msg, ten_extension_t *from_extension) {
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  TEN_ASSERT(from_extension, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(from_extension, true),
             "Invalid argument.");

  ten_loc_t *dest_loc = ten_msg_get_first_dest_loc(msg);
  TEN_ASSERT(dest_loc, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(dest_loc), "Should not happen.");

  ten_engine_t *engine = ten_extension_get_belonging_engine(from_extension);
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false),
             "Invalid use of engine %p.", engine);

  ten_app_t *app = engine->app;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid use of app %p.",
             app);

  if (app->is_standalone_test_app &&
      app->standalone_test_mode == TEN_EXTENSION_TESTER_TEST_MODE_SINGLE) {
    bool is_test_extension = from_extension->is_standalone_test_extension;
    const char *target_extension_name = NULL;

    if (is_test_extension) {
      target_extension_name =
          ten_string_get_raw_str(&app->standalone_tested_target_name);
    } else {
      target_extension_name = TEN_STR_TEN_TEST_EXTENSION;
    }

    ten_loc_set(dest_loc, ten_app_get_uri(app),
                ten_engine_get_id(engine, false), target_extension_name);
  }
}

bool ten_add_msg_dest_for_standalone_test_scenario(
    ten_shared_ptr_t *msg, ten_extension_t *from_extension) {
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  TEN_ASSERT(from_extension, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(from_extension, true),
             "Invalid argument.");

  ten_engine_t *engine = ten_extension_get_belonging_engine(from_extension);
  TEN_ASSERT(engine, "Invalid argument.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false),
             "Invalid use of engine %p.", engine);

  ten_app_t *app = engine->app;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid use of app %p.",
             app);

  if (app->is_standalone_test_app &&
      app->standalone_test_mode == TEN_EXTENSION_TESTER_TEST_MODE_SINGLE) {
    const char *target_extension_name = NULL;

    if (from_extension->is_standalone_test_extension) {
      target_extension_name =
          ten_string_get_raw_str(&app->standalone_tested_target_name);
    } else {
      target_extension_name = TEN_STR_TEN_TEST_EXTENSION;
    }

    ten_msg_add_dest(msg, ten_app_get_uri(app),
                     ten_engine_get_id(engine, false), target_extension_name);

    return true;
  }

  return false;
}
