//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/protocol/integrated/retry.h"

void ten_protocol_integrated_retry_config_init(
    ten_protocol_integrated_retry_config_t *self) {
  self->enable = false;
  self->max_retries = 0;
  self->interval_ms = 0;
}
