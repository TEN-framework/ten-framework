//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#if defined(TEN_ENABLE_TEN_RUST_APIS)

#include "include_internal/ten_runtime/extension_thread/telemetry.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/service_hub/service_hub.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_result/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_rust/ten_rust.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_utils/lib/time.h"

static MetricHandle *
ten_extension_thread_get_metric_extension_thread_msg_queue_stay_time_us(
    ten_extension_thread_t *self, const char **app_uri, const char **graph_id,
    const char **extension_group_name) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(app_uri && graph_id && extension_group_name, "Invalid argument.");

  *extension_group_name =
      ten_extension_group_get_name(self->extension_group, true);

  ten_extension_context_t *extension_context = self->extension_context;
  TEN_ASSERT(extension_context, "Should not happen.");
  // When the extension thread is still running, this instance will definitely
  // exist, and since the current operation does not involve any write actions,
  // so it is safe.
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, false),
             "Should not happen.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine, "Should not happen.");
  // When the extension thread is still running, this instance will definitely
  // exist, and since the current operation does not involve any write actions,
  // so it is safe.
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  *graph_id = ten_engine_get_id(engine, false);

  ten_app_t *app = engine->app;
  TEN_ASSERT(app, "Should not happen.");
  // When the extension thread is still running, this instance will definitely
  // exist, and since the current operation does not involve any write actions,
  // so it is safe.
  TEN_ASSERT(ten_app_check_integrity(app, false), "Should not happen.");

  *app_uri = ten_app_get_uri(app);

  return app->service_hub.metric_extension_thread_msg_queue_stay_time_us;
}

void ten_extension_thread_record_extension_thread_msg_queue_stay_time(
    ten_extension_thread_t *self, int64_t msg_timestamp) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  const char *app_uri = NULL;
  const char *graph_id = NULL;
  const char *extension_group_name = NULL;

  MetricHandle *extension_thread_msg_queue_stay_time =
      ten_extension_thread_get_metric_extension_thread_msg_queue_stay_time_us(
          self, &app_uri, &graph_id, &extension_group_name);
  if (extension_thread_msg_queue_stay_time) {
    int64_t duration_us = ten_current_time_us() - msg_timestamp;

    const char *label_values[] = {app_uri, graph_id, extension_group_name};

    ten_metric_histogram_observe(extension_thread_msg_queue_stay_time,
                                 (double)duration_us, label_values, 3);
  }
}

void ten_extension_record_lifecycle_duration(ten_extension_t *self,
                                             const char *stage,
                                             int64_t duration_us) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);
  TEN_ASSERT(stage, "Invalid argument.");

  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread &&
                 ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_context_t *extension_context =
      extension_thread->extension_context;
  TEN_ASSERT(extension_context && ten_extension_context_check_integrity(
                                      extension_context, false),
             "Should not happen.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
             "Should not happen.");

  ten_app_t *app = engine->app;
  TEN_ASSERT(app && ten_app_check_integrity(app, false), "Should not happen.");

  MetricHandle *metric_lifecycle =
      app->service_hub.metric_extension_lifecycle_duration_us;
  if (!metric_lifecycle) {
    // Metrics not enabled or not created, skip recording.
    return;
  }

  const char *app_uri = ten_app_get_uri(app);
  const char *graph_id = ten_engine_get_id(engine, false);
  const char *extension_name = ten_extension_get_name(self, true);

  const char *label_values[] = {app_uri, graph_id, extension_name, stage};

  ten_metric_gauge_set(metric_lifecycle, (double)duration_us, label_values, 4);
}

void ten_extension_record_cmd_processing_duration(ten_extension_t *self,
                                                  ten_shared_ptr_t *cmd_result,
                                                  int64_t on_cmd_start_us) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(cmd_result, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(cmd_result), "Invalid argument.");
  TEN_ASSERT(ten_msg_get_type(cmd_result) == TEN_MSG_TYPE_CMD_RESULT,
             "Should be cmd_result.");

  // Check if this is the final result.
  if (!ten_cmd_result_is_final(cmd_result, NULL)) {
    // Only record metrics for final results.
    return;
  }

  // Get app instance to access the metric.
  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread &&
                 ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_context_t *extension_context =
      extension_thread->extension_context;
  TEN_ASSERT(extension_context && ten_extension_context_check_integrity(
                                      extension_context, false),
             "Should not happen.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
             "Should not happen.");

  ten_app_t *app = engine->app;
  TEN_ASSERT(app && ten_app_check_integrity(app, false), "Should not happen.");

  MetricHandle *metric =
      app->service_hub.metric_extension_cmd_processing_duration_us;
  if (!metric) {
    // Metrics not enabled or not created, skip recording.
    return;
  }

  int64_t return_result_us = ten_current_time_us();
  int64_t duration_us = return_result_us - on_cmd_start_us;

  // Get extension location (app_uri, graph_id, extension_name).
  const char *app_uri = ten_app_get_uri(app);
  const char *graph_id = ten_engine_get_id(engine, false);
  const char *extension_name = ten_extension_get_name(self, true);

  // Get original cmd name from the cmd_result.
  ten_cmd_result_t *raw_cmd_result =
      (ten_cmd_result_t *)ten_msg_get_raw_msg(cmd_result);
  const char *original_cmd_name =
      ten_value_peek_raw_str(&raw_cmd_result->original_cmd_name, NULL);

  const char *label_values[] = {app_uri, graph_id, extension_name,
                                original_cmd_name};

  ten_metric_histogram_observe(metric, (double)duration_us, label_values, 4);
}

void ten_extension_record_callback_execution_duration(ten_extension_t *self,
                                                      const char *msg_type,
                                                      const char *msg_name,
                                                      int64_t duration_us) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(msg_type, "Invalid argument.");
  TEN_ASSERT(msg_name, "Invalid argument.");

  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread &&
                 ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_context_t *extension_context =
      extension_thread->extension_context;
  TEN_ASSERT(extension_context && ten_extension_context_check_integrity(
                                      extension_context, false),
             "Should not happen.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine && ten_engine_check_integrity(engine, false),
             "Should not happen.");

  ten_app_t *app = engine->app;
  TEN_ASSERT(app && ten_app_check_integrity(app, false), "Should not happen.");

  MetricHandle *metric =
      app->service_hub.metric_extension_callback_execution_duration_us;
  if (!metric) {
    // Metrics not enabled or not created, skip recording.
    return;
  }

  // Get extension location (app_uri, graph_id, extension_name).
  const char *app_uri = ten_app_get_uri(app);
  const char *graph_id = ten_engine_get_id(engine, false);
  const char *extension_name = ten_extension_get_name(self, true);

  const char *label_values[] = {app_uri, graph_id, extension_name, msg_type,
                                msg_name};

  ten_metric_histogram_observe(metric, (double)duration_us, label_values, 5);
}

#endif
