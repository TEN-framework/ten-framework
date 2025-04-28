//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/log/log.h"

TEN_UTILS_PRIVATE_API char ten_log_level_char(TEN_LOG_LEVEL level);

TEN_UTILS_PRIVATE_API void ten_log_set_output_level(ten_log_t *self,
                                                    TEN_LOG_LEVEL level);
