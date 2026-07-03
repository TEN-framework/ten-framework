//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/string.h"

TEN_UTILS_PRIVATE_API void ten_string_init_from_va_list(ten_string_t *self,
                                                        const char *fmt,
                                                        va_list ap);
