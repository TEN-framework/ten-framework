//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_utils/io/runloop.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/test/extension_tester.h"
#include "ten_utils/lib/signature.h"

#define TEN_EXTENSION_TESTER_SIGNATURE 0x2343E0B8559B7147U

// 3min by default.
#define TEN_EXTENSION_TESTER_DEFAULT_TIMEOUT_US \
  ((uint64_t)(3 * 60 * 1000 * 1000))

typedef struct ten_extension_tester_t ten_extension_tester_t;
typedef struct ten_env_tester_t ten_env_tester_t;
typedef struct ten_timer_t ten_timer_t;

typedef struct ten_extension_tester_test_graph_info_t {
  TEN_EXTENSION_TESTER_TEST_MODE test_mode;

  union {
    struct {
      ten_string_t addon_name;
      ten_string_t property_json;
    } single;

    struct {
      ten_string_t graph_json;
    } graph;
  } test_target;
} ten_extension_tester_test_graph_info_t;

struct ten_extension_tester_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_thread_t *test_app_thread;
  ten_env_proxy_t *test_app_ten_env_proxy;
  ten_event_t *test_app_ten_env_proxy_create_completed;

  ten_env_proxy_t *test_extension_ten_env_proxy;
  ten_event_t *test_extension_ten_env_proxy_create_completed;

  ten_extension_tester_test_graph_info_t test_graph_info;

  ten_string_t test_app_property_json;

  ten_extension_tester_on_init_func_t on_init;
  ten_extension_tester_on_start_func_t on_start;
  ten_extension_tester_on_stop_func_t on_stop;
  ten_extension_tester_on_deinit_func_t on_deinit;
  ten_extension_tester_on_cmd_func_t on_cmd;
  ten_extension_tester_on_data_func_t on_data;
  ten_extension_tester_on_audio_frame_func_t on_audio_frame;
  ten_extension_tester_on_video_frame_func_t on_video_frame;

  ten_env_tester_t *ten_env_tester;
  ten_runloop_t *tester_runloop;

  ten_error_t test_result;

  // Timeout
  uint64_t timeout_us;  // microseconds
  ten_timer_t *timeout_timer;

  void *user_data;
};

TEN_RUNTIME_API bool ten_extension_tester_check_integrity(
    ten_extension_tester_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API bool ten_extension_tester_thread_call_by_me(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_set_test_result(
    ten_extension_tester_t *self, ten_error_t *test_result);

TEN_RUNTIME_PRIVATE_API bool ten_extension_tester_could_be_closed(
    ten_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_tester_do_close(
    ten_extension_tester_t *self);
