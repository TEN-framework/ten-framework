//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

TEN_UTILS_API void *ten_shm_map(const char *name, size_t size);

TEN_UTILS_API size_t ten_shm_get_size(void *addr);

TEN_UTILS_API void ten_shm_unmap(void *addr);

TEN_UTILS_API void ten_shm_unlink(const char *name);
