//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

typedef void *ten_atomic_ptr_t;

TEN_UTILS_API void *ten_atomic_ptr_load(volatile ten_atomic_ptr_t *a);

TEN_UTILS_API void ten_atomic_ptr_store(volatile ten_atomic_ptr_t *a, void *v);
