//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>
#include <stdint.h>

TEN_RUNTIME_PRIVATE_API void ten_encrypt_log_data(uint8_t *data,
                                                  size_t data_len,
                                                  void *user_data);

TEN_RUNTIME_PRIVATE_API void ten_encrypt_log_deinit(void *user_data);
