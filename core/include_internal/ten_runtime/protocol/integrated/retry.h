//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ten_protocol_integrated_t ten_protocol_integrated_t;

typedef struct ten_protocol_integrated_retry_config_t {
  // Whether to enable the retry mechanism.
  bool enable;

  // The max retry times.
  uint32_t max_retries;

  // The interval between retries.
  uint32_t interval_ms;
} ten_protocol_integrated_retry_config_t;

TEN_RUNTIME_PRIVATE_API void ten_protocol_integrated_retry_config_init(
    ten_protocol_integrated_retry_config_t *self);
