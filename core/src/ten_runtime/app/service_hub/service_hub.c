//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/service_hub/service_hub.h"

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

  // Initialize default values.
  const char *telemetry_host = NULL;
  uint32_t telemetry_port = 0;
  const char *api_host = NULL;
  uint32_t api_port = 0;

  // Get the telemetry configuration.
  ten_value_t *telemetry_value =
      ten_value_object_peek(value, TEN_STR_TELEMETRY);
  if (telemetry_value && ten_value_is_object(telemetry_value)) {
    ten_value_t *enabled_value =
        ten_value_object_peek(telemetry_value, TEN_STR_ENABLED);
    if (enabled_value && ten_value_is_bool(enabled_value) &&
        ten_value_get_bool(enabled_value, NULL)) {
      // Get host and port for telemetry.
      telemetry_host = TEN_SERVICE_HUB_DEFAULT_HOST;
      telemetry_port = TEN_SERVICE_HUB_DEFAULT_PORT;

      ten_value_t *host_value =
          ten_value_object_peek(telemetry_value, TEN_STR_HOST);
      if (host_value && ten_value_is_string(host_value)) {
        telemetry_host = ten_value_peek_raw_str(host_value, NULL);
      }

      ten_value_t *port_value =
          ten_value_object_peek(telemetry_value, TEN_STR_PORT);
      if (port_value) {
        telemetry_port = ten_value_get_uint32(port_value, NULL);
      }
    }
  }

  // Get the API configuration.
  ten_value_t *api_value = ten_value_object_peek(value, TEN_STR_API);
  if (api_value && ten_value_is_object(api_value)) {
    ten_value_t *enabled_value =
        ten_value_object_peek(api_value, TEN_STR_ENABLED);
    if (enabled_value && ten_value_is_bool(enabled_value) &&
        ten_value_get_bool(enabled_value, NULL)) {
      // Get host and port for API.
      api_host = TEN_SERVICE_HUB_DEFAULT_HOST;
      api_port = TEN_SERVICE_HUB_DEFAULT_PORT;

      ten_value_t *host_value = ten_value_object_peek(api_value, TEN_STR_HOST);
      if (host_value && ten_value_is_string(host_value)) {
        api_host = ten_value_peek_raw_str(host_value, NULL);
      }

      ten_value_t *port_value = ten_value_object_peek(api_value, TEN_STR_PORT);
      if (port_value) {
        api_port = ten_value_get_uint32(port_value, NULL);
      }
    }
  }

  // Create service hub with collected parameters.
  if (telemetry_host != NULL || api_host != NULL) {
    self->service_hub.service_hub = ten_service_hub_create(
        telemetry_host, telemetry_port, api_host, api_port, self);

    if (!self->service_hub.service_hub) {
      TEN_LOGE("Failed to create service hub");
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    } else {
      if (telemetry_host != NULL && api_host != NULL) {
        TEN_LOGI("Created service hub with telemetry at %s:%d and API at %s:%d",
                 telemetry_host, telemetry_port, api_host, api_port);
      } else if (telemetry_host != NULL) {
        TEN_LOGI("Created service hub with telemetry only at %s:%d",
                 telemetry_host, telemetry_port);
      } else {
        TEN_LOGI("Created service hub with API only at %s:%d", api_host,
                 api_port);
      }

      // Create metrics if telemetry is enabled.
      if (telemetry_host != NULL) {
        ten_app_service_hub_create_metric(self);
      }
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
