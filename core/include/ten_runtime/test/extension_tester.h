//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_extension_tester_t ten_extension_tester_t;
typedef struct ten_env_tester_t ten_env_tester_t;

typedef enum TEN_EXTENSION_TESTER_TEST_MODE {
  TEN_EXTENSION_TESTER_TEST_MODE_INVALID,

  TEN_EXTENSION_TESTER_TEST_MODE_SINGLE,
  TEN_EXTENSION_TESTER_TEST_MODE_GRAPH,
} TEN_EXTENSION_TESTER_TEST_MODE;

typedef void (*ten_extension_tester_on_init_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env);

typedef void (*ten_extension_tester_on_start_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env);

typedef void (*ten_extension_tester_on_stop_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env);

typedef void (*ten_extension_tester_on_deinit_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env);

typedef void (*ten_extension_tester_on_cmd_func_t)(ten_extension_tester_t *self,
                                                   ten_env_tester_t *ten_env,
                                                   ten_shared_ptr_t *cmd);

typedef void (*ten_extension_tester_on_data_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env,
    ten_shared_ptr_t *data);

typedef void (*ten_extension_tester_on_audio_frame_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env,
    ten_shared_ptr_t *audio_frame);

typedef void (*ten_extension_tester_on_video_frame_func_t)(
    ten_extension_tester_t *self, ten_env_tester_t *ten_env,
    ten_shared_ptr_t *video_frame);

TEN_RUNTIME_API ten_extension_tester_t *ten_extension_tester_create(
    ten_extension_tester_on_init_func_t on_init,
    ten_extension_tester_on_start_func_t on_start,
    ten_extension_tester_on_stop_func_t on_stop,
    ten_extension_tester_on_deinit_func_t on_deinit,
    ten_extension_tester_on_cmd_func_t on_cmd,
    ten_extension_tester_on_data_func_t on_data,
    ten_extension_tester_on_audio_frame_func_t on_audio_frame,
    ten_extension_tester_on_video_frame_func_t on_video_frame);

TEN_RUNTIME_API void ten_extension_tester_destroy(ten_extension_tester_t *self);

// Testing a single extension, all messages input by the tester will be directed
// to this extension, and all outputs from the extension will be sent back to
// the tester.
TEN_RUNTIME_API void ten_extension_tester_set_test_mode_single(
    ten_extension_tester_t *self, const char *addon_name,
    const char *property_json_str);

// Testing a complete graph which must contain exactly one proxy extension. All
// messages input by the tester will be directed to this proxy extension, and
// all outputs from the proxy extension will be sent back to the tester.
TEN_RUNTIME_API void ten_extension_tester_set_test_mode_graph(
    ten_extension_tester_t *self, const char *graph_json);

TEN_RUNTIME_API void ten_extension_tester_init_test_app_property_from_json(
    ten_extension_tester_t *self, const char *property_json_str);

TEN_RUNTIME_API bool ten_extension_tester_run(ten_extension_tester_t *self);

TEN_RUNTIME_API ten_env_tester_t *ten_extension_tester_get_ten_env_tester(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_test_extension_init(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_test_extension_start(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_test_extension_stop(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_test_extension_deinit(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_init_done(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_start_done(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_stop_done(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_on_deinit_done(
    ten_extension_tester_t *self);
