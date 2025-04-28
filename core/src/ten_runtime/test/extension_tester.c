//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/test/extension_tester.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/msg_interface/common.h"
#include "include_internal/ten_runtime/extension_group/builtin/builtin_extension_group.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "include_internal/ten_runtime/test/test_app.h"
#include "include_internal/ten_runtime/test/test_extension.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_str.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_extension_tester_check_integrity(ten_extension_tester_t *self,
                                          bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_TESTER_SIGNATURE) {
    TEN_ASSERT(0,
               "Failed to pass extension_thread signature checking: %" PRId64,
               self->signature);
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

bool ten_extension_tester_thread_call_by_me(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, false),
             "Invalid argument.");

  return ten_thread_equal(NULL, ten_sanitizer_thread_check_get_belonging_thread(
                                    &self->thread_check));
}

ten_extension_tester_t *ten_extension_tester_create(
    ten_extension_tester_on_init_func_t on_init,
    ten_extension_tester_on_start_func_t on_start,
    ten_extension_tester_on_stop_func_t on_stop,
    ten_extension_tester_on_deinit_func_t on_deinit,
    ten_extension_tester_on_cmd_func_t on_cmd,
    ten_extension_tester_on_data_func_t on_data,
    ten_extension_tester_on_audio_frame_func_t on_audio_frame,
    ten_extension_tester_on_video_frame_func_t on_video_frame) {
  ten_extension_tester_t *self = TEN_MALLOC(sizeof(ten_extension_tester_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->binding_handle.me_in_target_lang = self;

  ten_signature_set(&self->signature, TEN_EXTENSION_TESTER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->on_init = on_init;
  self->on_start = on_start;
  self->on_stop = on_stop;
  self->on_deinit = on_deinit;
  self->on_cmd = on_cmd;
  self->on_data = on_data;
  self->on_audio_frame = on_audio_frame;
  self->on_video_frame = on_video_frame;

  self->ten_env_tester = ten_env_tester_create(self);
  self->tester_runloop = ten_runloop_create(NULL);

  self->test_extension_ten_env_proxy = NULL;
  self->test_extension_ten_env_proxy_create_completed = ten_event_create(0, 1);

  self->test_app_ten_env_proxy = NULL;
  self->test_app_ten_env_proxy_create_completed = ten_event_create(0, 1);
  TEN_STRING_INIT(self->test_app_property_json);

  self->test_app_thread = NULL;
  self->user_data = NULL;

  self->test_mode = TEN_EXTENSION_TESTER_TEST_MODE_INVALID;

  return self;
}

void ten_extension_tester_set_test_mode_single(ten_extension_tester_t *self,
                                               const char *addon_name,
                                               const char *property_json_str) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(addon_name, "Invalid argument.");

  self->test_mode = TEN_EXTENSION_TESTER_TEST_MODE_SINGLE;
  ten_string_init_from_c_str_with_size(&self->test_target.addon.addon_name,
                                       addon_name, strlen(addon_name));

  if (property_json_str && strlen(property_json_str) > 0) {
    ten_error_t err;
    TEN_ERROR_INIT(err);

    ten_json_t *json = ten_json_from_string(property_json_str, &err);
    if (json) {
      ten_json_destroy(json);
    } else {
      TEN_ASSERT(0, "Failed to parse property json: %s",
                 ten_error_message(&err));
    }

    ten_error_deinit(&err);

    ten_string_init_from_c_str_with_size(&self->test_target.addon.property_json,
                                         property_json_str,
                                         strlen(property_json_str));
  } else {
    const char *empty_json = "{}";
    ten_string_init_from_c_str_with_size(&self->test_target.addon.property_json,
                                         empty_json, strlen(empty_json));
  }
}

void ten_extension_tester_set_test_mode_graph(ten_extension_tester_t *self,
                                              const char *graph_json) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(graph_json, "Invalid argument.");

  self->test_mode = TEN_EXTENSION_TESTER_TEST_MODE_GRAPH;
  ten_string_init_from_c_str_with_size(&self->test_target.graph.graph_json,
                                       graph_json, strlen(graph_json));
}

void ten_extension_tester_init_test_app_property_from_json(
    ten_extension_tester_t *self, const char *property_json_str) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(property_json_str, "Invalid argument.");

  ten_string_set_formatted(&self->test_app_property_json, "%s",
                           property_json_str);
}

static void ten_extension_tester_destroy_test_target(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: In TEN world, the destroy operations need to be performed
      // in any threads.
      ten_extension_tester_check_integrity(self, false), "Invalid argument.");

  if (self->test_mode == TEN_EXTENSION_TESTER_TEST_MODE_SINGLE) {
    ten_string_deinit(&self->test_target.addon.addon_name);
    ten_string_deinit(&self->test_target.addon.property_json);
  } else if (self->test_mode == TEN_EXTENSION_TESTER_TEST_MODE_GRAPH) {
    ten_string_deinit(&self->test_target.graph.graph_json);
  }
}

void ten_extension_tester_destroy(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: In TEN world, the destroy operations need to be performed
      // in any threads.
      ten_extension_tester_check_integrity(self, false), "Invalid argument.");

  // The `ten_env_proxy` of `test_app` should be released in the tester task
  // triggered by the `deinit` of `test_app`.
  TEN_ASSERT(self->test_app_ten_env_proxy == NULL, "Should not happen.");

  TEN_LOGI("Destroying extension_tester.");

  if (self->test_app_ten_env_proxy_create_completed) {
    ten_event_destroy(self->test_app_ten_env_proxy_create_completed);
  }

  // `ten_env_proxy` of `test_extension` should be released in the tester task
  // triggered by the `deinit` of `test_extension`.
  TEN_ASSERT(self->test_extension_ten_env_proxy == NULL, "Should not happen.");

  if (self->test_extension_ten_env_proxy_create_completed) {
    ten_event_destroy(self->test_extension_ten_env_proxy_create_completed);
  }

  ten_thread_join(self->test_app_thread, -1);

  ten_extension_tester_destroy_test_target(self);

  ten_string_deinit(&self->test_app_property_json);

  ten_env_tester_destroy(self->ten_env_tester);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  ten_runloop_destroy(self->tester_runloop);
  self->tester_runloop = NULL;

  TEN_FREE(self);
}

static void test_app_start_graph_result_handler(ten_env_t *ten_env,
                                                ten_shared_ptr_t *cmd_result,
                                                void *user_data,
                                                ten_error_t *err) {
  TEN_ASSERT(cmd_result, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(cmd_result), "Invalid argument.");

  TEN_STATUS_CODE status_code = ten_cmd_result_get_status_code(cmd_result);

  if (status_code == TEN_STATUS_CODE_OK) {
    TEN_LOGI("Successfully started standalone testing graph.");
  } else {
    TEN_LOGE("Failed to start standalone testing graph, status_code: %d",
             status_code);
    exit(EXIT_FAILURE);
  }
}

static void test_app_ten_env_send_start_graph_cmd(ten_env_t *ten_env,
                                                  void *user_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *app = ten_env->attached_target.app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_shared_ptr_t *cmd = user_data;
  TEN_ASSERT(cmd && ten_msg_check_integrity(cmd), "Should not happen.");

  // TODO(Wei): Currently, the app does not have a centralized place to handle
  // all `path_table` operations. Therefore, the lowest-level approach is used
  // here to add the result handler and `dispatch_msg`, rather than using the
  // high-level API `ten_env_send_cmd`. In the future, it will be necessary to
  // consider whether a general mechanism can be implemented to better handle
  // the app's command routing.

  // Because `extension_tester` needs to receive the `cmd_result` of the sent
  // `start_graph` command, the `start_graph` command must have a `cmd_id` so
  // that the out_path mechanism of the app's path table can take effect. This
  // allows the returned `cmd_result` to find the correct out_path from the path
  // table using the `cmd_id`.
  ten_cmd_base_gen_new_cmd_id_forcibly(cmd);

  // Set the source location of `msg` to the URI of the `app`, so that the
  // `cmd_result` of the `start_graph` command can ultimately be returned to
  // this `app` and processed by the `out path`, enabling the invocation of the
  // result handler specified below.
  ten_msg_set_src(cmd, ten_app_get_uri(app), NULL, NULL, NULL);

  bool rc = ten_msg_clear_and_set_dest(cmd, ten_app_get_uri(app), NULL, NULL,
                                       NULL, NULL);
  TEN_ASSERT(rc, "Should not happen.");

  // Set up a result handler so that the returned `cmd_result` can be
  // processed using the `path_table` mechanism.
  ten_cmd_base_set_result_handler(cmd, test_app_start_graph_result_handler,
                                  NULL);

  ten_path_t *out_path =
      (ten_path_t *)ten_path_table_add_out_path(app->path_table, cmd);
  TEN_ASSERT(out_path && ten_path_check_integrity(out_path, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  rc = ten_app_dispatch_msg(app, cmd, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);
}

static void ten_extension_tester_create_and_start_graph(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(self->test_mode != TEN_EXTENSION_TESTER_TEST_MODE_INVALID,
             "Invalid test mode.");
  TEN_ASSERT(self->test_app_ten_env_proxy, "Invalid test app ten_env_proxy.");

  ten_shared_ptr_t *start_graph_cmd = ten_cmd_start_graph_create();
  TEN_ASSERT(start_graph_cmd, "Should not happen.");

  bool rc = false;

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (self->test_mode == TEN_EXTENSION_TESTER_TEST_MODE_SINGLE) {
    TEN_ASSERT(ten_string_check_integrity(&self->test_target.addon.addon_name),
               "Invalid test target.");

    const char *addon_name =
        ten_string_get_raw_str(&self->test_target.addon.addon_name);

    const char *property_json_str =
        ten_string_get_raw_str(&self->test_target.addon.property_json);

    ten_string_t graph_json_str;
    ten_string_init_formatted(&graph_json_str,
                              "{\
           \"nodes\": [{\
              \"type\": \"extension\",\
              \"name\": \"ten:test_extension\",\
              \"addon\": \"ten:test_extension\",\
              \"extension_group\": \"test_extension_group_1\",\
              \"app\": \"localhost\"\
           },{\
              \"type\": \"extension\",\
              \"name\": \"%s\",\
              \"addon\": \"%s\",\
              \"extension_group\": \"test_extension_group_2\",\
              \"app\": \"localhost\",\
              \"property\": %s\
           }],\
           \"connections\": [{\
             \"app\": \"localhost\",\
             \"extension_group\": \"test_extension_group_1\",\
             \"extension\": \"ten:test_extension\",\
             \"cmd\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_2\",\
                  \"extension\": \"%s\"\
               }]\
             }],\
             \"data\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_2\",\
                  \"extension\": \"%s\"\
               }]\
             }],\
             \"video_frame\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_2\",\
                  \"extension\": \"%s\"\
               }]\
             }],\
             \"audio_frame\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_2\",\
                  \"extension\": \"%s\"\
               }]\
             }]\
           },{\
             \"app\": \"localhost\",\
             \"extension_group\": \"test_extension_group_2\",\
             \"extension\": \"%s\",\
             \"cmd\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_1\",\
                  \"extension\": \"ten:test_extension\"\
               }]\
             }],\
             \"data\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_1\",\
                  \"extension\": \"ten:test_extension\"\
               }]\
             }],\
             \"video_frame\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_1\",\
                  \"extension\": \"ten:test_extension\"\
               }]\
             }],\
             \"audio_frame\": [{\
               \"name\": \"*\",\
               \"dest\": [{\
                  \"app\": \"localhost\",\
                  \"extension_group\": \"test_extension_group_1\",\
                  \"extension\": \"ten:test_extension\"\
               }]\
             }]\
           }]\
       }",
                              addon_name, addon_name, property_json_str,
                              addon_name, addon_name, addon_name, addon_name,
                              addon_name);
    rc = ten_cmd_start_graph_set_graph_from_json_str(
        start_graph_cmd, ten_string_get_raw_str(&graph_json_str), &err);
    TEN_ASSERT(rc, "Should not happen.");

    ten_string_deinit(&graph_json_str);
  } else if (self->test_mode == TEN_EXTENSION_TESTER_TEST_MODE_GRAPH) {
    TEN_ASSERT(ten_string_check_integrity(&self->test_target.graph.graph_json),
               "Invalid test target.");
    TEN_ASSERT(&self->test_target.graph.graph_json, "Should not happen.");

    rc = ten_cmd_start_graph_set_graph_from_json_str(
        start_graph_cmd,
        ten_string_get_raw_str(&self->test_target.graph.graph_json), &err);
    TEN_ASSERT(rc, "Should not happen.");
  }

  rc = ten_env_proxy_notify(self->test_app_ten_env_proxy,
                            test_app_ten_env_send_start_graph_cmd,
                            start_graph_cmd, false, &err);
  TEN_ASSERT(rc, "Should not happen.");

  // Wait for the tester extension to create the `ten_env_proxy`.
  ten_event_wait(self->test_extension_ten_env_proxy_create_completed, -1);

  ten_event_destroy(self->test_extension_ten_env_proxy_create_completed);
  self->test_extension_ten_env_proxy_create_completed = NULL;
}

static void ten_extension_tester_create_and_run_app(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  // Create the tester app.
  self->test_app_thread = ten_thread_create(
      "test app thread", ten_builtin_test_app_thread_main, self);

  // Wait until the tester app is started successfully.
  ten_event_wait(self->test_app_ten_env_proxy_create_completed, -1);

  ten_event_destroy(self->test_app_ten_env_proxy_create_completed);
  self->test_app_ten_env_proxy_create_completed = NULL;

  TEN_ASSERT(self->test_app_ten_env_proxy,
             "test_app should have been created its ten_env_proxy.");
}

void ten_extension_tester_on_init_done(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid use of extension_tester %p.", self);
  TEN_ASSERT(self->test_extension_ten_env_proxy,
             "The test extension should have been created its ten_env_proxy.");

  TEN_LOGI("tester on_init() done.");

  bool rc = ten_env_proxy_notify(
      self->test_extension_ten_env_proxy,
      ten_builtin_test_extension_ten_env_notify_on_init_done, NULL, false,
      NULL);
  TEN_ASSERT(rc, "Should not happen.");
}

void ten_extension_tester_on_start_done(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid use of extension_tester %p.", self);
  TEN_ASSERT(self->test_extension_ten_env_proxy,
             "The test extension should have been created its ten_env_proxy.");

  TEN_LOGI("tester on_start() done.");

  bool rc = ten_env_proxy_notify(
      self->test_extension_ten_env_proxy,
      ten_builtin_test_extension_ten_env_notify_on_start_done, NULL, false,
      NULL);
  TEN_ASSERT(rc, "Should not happen.");
}

void ten_extension_tester_on_stop_done(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(self->test_extension_ten_env_proxy,
             "The test extension should have been created its ten_env_proxy.");

  TEN_LOGI("tester on_stop() done.");

  bool rc = ten_env_proxy_notify(
      self->test_extension_ten_env_proxy,
      ten_builtin_test_extension_ten_env_notify_on_stop_done, NULL, false,
      NULL);
  TEN_ASSERT(rc, "Should not happen.");
}

void ten_extension_tester_on_deinit_done(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(self->test_extension_ten_env_proxy,
             "The test extension should have been created its ten_env_proxy.");

  TEN_LOGI("tester on_deinit() done.");

  bool rc = ten_env_proxy_notify(
      self->test_extension_ten_env_proxy,
      ten_builtin_test_extension_ten_env_notify_on_deinit_done, NULL, false,
      NULL);
  TEN_ASSERT(rc, "Should not happen.");

  // Since the tester uses the extension's `ten_env_proxy` to interact with
  // `test_extension`, it is necessary to release the extension's
  // `ten_env_proxy` within the tester thread to ensure thread safety.
  //
  // Releasing the extension's `ten_env_proxy` within the tester thread also
  // guarantees that `test_extension` is still active at that time (As long as
  // the `ten_env_proxy` exists, the extension will not be destroyed.), ensuring
  // that all operations using the extension's `ten_env_proxy` before the
  // releasing of ten_env_proxy are valid.
  TEN_LOGI("Releasing test extension's ten_env_proxy.");
  rc = ten_env_proxy_release(self->test_extension_ten_env_proxy, NULL);
  TEN_ASSERT(rc, "Should not happen.");

  self->test_extension_ten_env_proxy = NULL;
}

void ten_extension_tester_on_test_extension_init(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  if (self->on_init) {
    self->on_init(self, self->ten_env_tester);
  } else {
    ten_extension_tester_on_init_done(self);
  }
}

void ten_extension_tester_on_test_extension_start(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  if (self->on_start) {
    self->on_start(self, self->ten_env_tester);
  } else {
    ten_extension_tester_on_start_done(self);
  }
}

void ten_extension_tester_on_test_extension_stop(ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  if (self->on_stop) {
    self->on_stop(self, self->ten_env_tester);
  } else {
    ten_extension_tester_on_stop_done(self);
  }
}

void ten_extension_tester_on_test_extension_deinit(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  if (self->on_deinit) {
    self->on_deinit(self, self->ten_env_tester);
  } else {
    ten_extension_tester_on_deinit_done(self);
  }
}

static void ten_extension_tester_on_first_task(void *self_,
                                               TEN_UNUSED void *arg) {
  ten_extension_tester_t *self = (ten_extension_tester_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_tester_create_and_run_app(self);
  ten_extension_tester_create_and_start_graph(self);
}

static void ten_extension_tester_inherit_thread_ownership(
    ten_extension_tester_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The correct threading ownership will be setup soon, so we can
  // _not_ check thread safety here.
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, false),
             "Invalid argument.");

  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &self->thread_check);
}

bool ten_extension_tester_run(ten_extension_tester_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function could be called in different threads other than
  // the creation thread.
  TEN_ASSERT(self && ten_extension_tester_check_integrity(self, false),
             "Invalid argument.");
  TEN_ASSERT(self->test_mode != TEN_EXTENSION_TESTER_TEST_MODE_INVALID,
             "Invalid test mode.");

  ten_extension_tester_inherit_thread_ownership(self);

  // Inject the task that calls the first task into the runloop of
  // extension_tester, ensuring that the first task is called within the
  // extension_tester thread to guarantee thread safety.
  int rc = ten_runloop_post_task_tail(
      self->tester_runloop, ten_extension_tester_on_first_task, self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to extension_tester's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  // Start the runloop of tester.
  ten_runloop_run(self->tester_runloop);

  return true;
}

ten_env_tester_t *ten_extension_tester_get_ten_env_tester(
    ten_extension_tester_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_tester_check_integrity(self, true),
             "Invalid argument.");

  return self->ten_env_tester;
}
