//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_runtime/app/app.h"
#include "ten_utils/value/value.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)

#define TEN_SERVICE_HUB_DEFAULT_HOST "0.0.0.0"
#define TEN_SERVICE_HUB_DEFAULT_PORT 49484

typedef struct ServiceHub ServiceHub;
typedef struct MetricHandle MetricHandle;

typedef struct ten_service_hub_t {
  ServiceHub *service_hub;

  MetricHandle *metric_extension_thread_msg_queue_stay_time_us;
} ten_service_hub_t;

TEN_RUNTIME_PRIVATE_API void ten_service_hub_init(ten_service_hub_t *self);

TEN_RUNTIME_PRIVATE_API void ten_app_deinit_service_hub(ten_app_t *self);

#endif

TEN_RUNTIME_PRIVATE_API bool ten_app_init_service_hub(ten_app_t *self,
                                                      ten_value_t *value);
