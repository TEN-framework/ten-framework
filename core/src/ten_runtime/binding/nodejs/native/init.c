//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/addon/addon.h"
#include "include_internal/ten_runtime/binding/nodejs/addon/addon_manager.h"
#include "include_internal/ten_runtime/binding/nodejs/app/app.h"
#include "include_internal/ten_runtime/binding/nodejs/error/error.h"
#include "include_internal/ten_runtime/binding/nodejs/extension/extension.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/audio_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/cmd_result.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/start_graph_cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/stop_graph_cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/cmd/trigger_life_cycle_cmd.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/data.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/msg.h"
#include "include_internal/ten_runtime/binding/nodejs/msg/video_frame.h"
#include "include_internal/ten_runtime/binding/nodejs/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/nodejs/test/env_tester.h"
#include "include_internal/ten_runtime/binding/nodejs/test/extension_tester.h"

static napi_value ten_runtime_nodejs_init(napi_env env, napi_value exports) {
  ten_nodejs_addon_module_init(env, exports);
  ten_nodejs_addon_manager_module_init(env, exports);
  ten_nodejs_app_module_init(env, exports);
  ten_nodejs_ten_env_module_init(env, exports);
  ten_nodejs_extension_module_init(env, exports);
  ten_nodejs_msg_module_init(env, exports);
  ten_nodejs_data_module_init(env, exports);
  ten_nodejs_cmd_module_init(env, exports);
  ten_nodejs_cmd_result_module_init(env, exports);
  ten_nodejs_cmd_start_graph_module_init(env, exports);
  ten_nodejs_cmd_stop_graph_module_init(env, exports);
  ten_nodejs_cmd_trigger_life_cycle_module_init(env, exports);
  ten_nodejs_video_frame_module_init(env, exports);
  ten_nodejs_audio_frame_module_init(env, exports);
  ten_nodejs_ten_env_tester_module_init(env, exports);
  ten_nodejs_extension_tester_module_init(env, exports);
  ten_nodejs_error_module_init(env, exports);

  return exports;
}

NAPI_MODULE(ten_runtime_nodejs, ten_runtime_nodejs_init)
