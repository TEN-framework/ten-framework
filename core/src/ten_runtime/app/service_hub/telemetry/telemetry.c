//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#if defined(TEN_ENABLE_TEN_RUST_APIS)

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_rust/ten_rust.h"

void ten_app_service_hub_create_metric(ten_app_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid use of app %p.",
             self);

  TEN_ASSERT(!self->service_hub.metric_extension_thread_msg_queue_stay_time_us,
             "Should not happen.");
  TEN_ASSERT(!self->service_hub.metric_extension_lifecycle_duration_us,
             "Should not happen.");
  TEN_ASSERT(!self->service_hub.metric_extension_cmd_processing_duration_us,
             "Should not happen.");
  TEN_ASSERT(!self->service_hub.metric_extension_callback_execution_duration_us,
             "Should not happen.");

  if (self->service_hub.service_hub) {
    const char *label_names[] = {"app_uri", "graph_id", "extension_group_name"};

    self->service_hub.metric_extension_thread_msg_queue_stay_time_us =
        ten_metric_create(
            self->service_hub.service_hub, 2,
            "extension_thread_msg_queue_stay_time",
            "The duration (in micro-seconds) that messages stay "
            "in the message queue of extension thread before being "
            "processed. Use this to identify overloaded extension threads.",
            label_names, 3);
    TEN_ASSERT(self->service_hub.metric_extension_thread_msg_queue_stay_time_us,
               "Should not happen.");

    const char *lifecycle_label_names[] = {"app_uri", "graph_id",
                                           "extension_name", "stage"};

    self->service_hub.metric_extension_lifecycle_duration_us =
        ten_metric_create(self->service_hub.service_hub, 1,
                          "extension_lifecycle_duration",
                          "The duration (in micro-seconds) of each extension "
                          "lifecycle stage (on_configure, on_init, on_start, "
                          "on_stop, on_deinit).",
                          lifecycle_label_names, 4);
    TEN_ASSERT(self->service_hub.metric_extension_lifecycle_duration_us,
               "Should not happen.");

    const char *cmd_processing_label_names[] = {"app_uri", "graph_id",
                                                "extension_name", "msg_name"};

    self->service_hub.metric_extension_cmd_processing_duration_us =
        ten_metric_create(self->service_hub.service_hub, 2,
                          "extension_cmd_processing_duration",
                          "The duration (in micro-seconds) from when an "
                          "extension starts processing a cmd (on_cmd called) "
                          "to when it returns the final result.",
                          cmd_processing_label_names, 4);
    TEN_ASSERT(self->service_hub.metric_extension_cmd_processing_duration_us,
               "Should not happen.");

    const char *callback_execution_label_names[] = {
        "app_uri", "graph_id", "extension_name", "msg_type", "msg_name"};

    self->service_hub.metric_extension_callback_execution_duration_us =
        ten_metric_create(
            self->service_hub.service_hub, 2,
            "extension_callback_execution_duration",
            "The duration (in micro-seconds) of extension callback function "
            "execution (on_cmd, on_data, on_audio_frame, on_video_frame). "
            "This helps identify blocking operations in callbacks that may "
            "cause the event loop to stall.",
            callback_execution_label_names, 5);
    TEN_ASSERT(
        self->service_hub.metric_extension_callback_execution_duration_us,
        "Should not happen.");
  }
}

void ten_app_service_hub_destroy_metric(ten_app_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  if (self->service_hub.metric_extension_thread_msg_queue_stay_time_us) {
    TEN_ASSERT(self->service_hub.service_hub, "Should not happen.");

    ten_metric_destroy(
        self->service_hub.metric_extension_thread_msg_queue_stay_time_us);
    self->service_hub.metric_extension_thread_msg_queue_stay_time_us = NULL;
  }

  if (self->service_hub.metric_extension_lifecycle_duration_us) {
    TEN_ASSERT(self->service_hub.service_hub, "Should not happen.");

    ten_metric_destroy(
        self->service_hub.metric_extension_lifecycle_duration_us);
    self->service_hub.metric_extension_lifecycle_duration_us = NULL;
  }

  if (self->service_hub.metric_extension_cmd_processing_duration_us) {
    TEN_ASSERT(self->service_hub.service_hub, "Should not happen.");

    ten_metric_destroy(
        self->service_hub.metric_extension_cmd_processing_duration_us);
    self->service_hub.metric_extension_cmd_processing_duration_us = NULL;
  }

  if (self->service_hub.metric_extension_callback_execution_duration_us) {
    TEN_ASSERT(self->service_hub.service_hub, "Should not happen.");

    ten_metric_destroy(
        self->service_hub.metric_extension_callback_execution_duration_us);
    self->service_hub.metric_extension_callback_execution_duration_us = NULL;
  }
}

#endif
