//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#if defined(_WIN32)
#define ten_alignof __alignof
#else
#include <stdalign.h>
#define ten_alignof alignof
#endif

#include <stddef.h>

// Utility for aligning addresses.
TEN_UTILS_API size_t ten_align_forward(size_t addr, size_t align);
