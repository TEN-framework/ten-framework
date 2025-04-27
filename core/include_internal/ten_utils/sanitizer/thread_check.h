//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/sanitizer/thread_check.h"

TEN_UTILS_API void ten_sanitizer_thread_check_init(
    ten_sanitizer_thread_check_t *self);
