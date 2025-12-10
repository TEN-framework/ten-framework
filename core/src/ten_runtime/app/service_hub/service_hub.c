//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/service_hub/service_hub.h"

#include "ten_utils/macro/memory.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/service_hub/telemetry/telemetry.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_rust/ten_rust.h"

void ten_service_hub_init(ten_service_hub_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  self->service_hub = NULL;
  self->metric_extension_thread_msg_queue_stay_time_us = NULL;
}

static bool is_telemetry_metrics_enabled(ten_value_t *value) {
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");
  TEN_ASSERT(ten_value_is_object(value), "Should not happen.");

  ten_value_t *telemetry_value =
      ten_value_object_peek(value, TEN_STR_TELEMETRY);
  if (telemetry_value && ten_value_is_object(telemetry_value)) {
    ten_value_t *telemetry_enabled_value =
        ten_value_object_peek(telemetry_value, TEN_STR_ENABLED);
    if (!telemetry_enabled_value ||
        !ten_value_is_bool(telemetry_enabled_value) ||
        !ten_value_get_bool(telemetry_enabled_value, NULL)) {
      return false;
    }

    ten_value_t *metrics_value =
        ten_value_object_peek(telemetry_value, TEN_STR_METRICS);
    if (metrics_value && ten_value_is_object(metrics_value)) {
      ten_value_t *enabled_value =
          ten_value_object_peek(metrics_value, TEN_STR_ENABLED);
      if (enabled_value && ten_value_is_bool(enabled_value) &&
          ten_value_get_bool(enabled_value, NULL)) {
        return true;
      }
    }
  }

  return false;
}

#endif

bool ten_app_init_service_hub(ten_app_t *self, ten_value_t *value) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_object(value)) {
    TEN_LOGE("Invalid value type for property: services. Expected an object.");
    return false;
  }

  // Serialize the entire services configuration to JSON string.
  // The Rust side will parse and extract what it needs.
  const char *services_config_json_str = NULL;
  ten_json_t services_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(value, &services_json);
  if (success) {
    bool must_free = false;
    services_config_json_str =
        ten_json_to_string(&services_json, NULL, &must_free);
  }
  ten_json_deinit(&services_json);

  // Create service hub if we have valid configuration.
  if (services_config_json_str) {
    self->service_hub.service_hub =
        ten_service_hub_create(services_config_json_str);

    // Clean up the JSON string
    TEN_FREE(services_config_json_str);

    if (!self->service_hub.service_hub) {
      TEN_LOGE("Failed to create service hub");
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }

    TEN_LOGI("Service hub created successfully");

    // Create metrics if telemetry is enabled.
    // Check if telemetry.metrics is enabled in the configuration.
    if (is_telemetry_metrics_enabled(value)) {
      ten_app_service_hub_create_metric(self);
    }
  }
#endif

  return true;
}

#if defined(TEN_ENABLE_TEN_RUST_APIS)

void ten_app_deinit_service_hub(ten_app_t *self) {
  if (self->service_hub.service_hub) {
    TEN_LOGD("[%s] Destroy service hub", ten_app_get_uri(self));

    ten_app_service_hub_destroy_metric(self);

    ten_service_hub_shutdown(self->service_hub.service_hub);
  }
}

#endif
