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

  if (self->service_hub.service_hub) {
    const char *label_names[] = {"app", "graph", "extension_group"};

    self->service_hub.metric_extension_thread_msg_queue_stay_time_us =
        ten_metric_create(self->service_hub.service_hub, 1,
                          "extension_thread_msg_queue_stay_time",
                          "The duration (in micro-seconds) that a message "
                          "instance stays in the message queue of extension "
                          "thread before being processed.",
                          label_names, 3);
    TEN_ASSERT(self->service_hub.metric_extension_thread_msg_queue_stay_time_us,
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
}

#endif
