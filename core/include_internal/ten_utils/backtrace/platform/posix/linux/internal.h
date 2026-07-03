//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

typedef struct ten_backtrace_t ten_backtrace_t;

TEN_UTILS_PRIVATE_API int ten_backtrace_capture_to_buffer(ten_backtrace_t *self,
                                                          char *buffer,
                                                          size_t buffer_size,
                                                          size_t skip);
