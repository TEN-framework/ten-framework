//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>

typedef uint64_t ten_signature_t;

TEN_UTILS_API void ten_signature_set(ten_signature_t *signature,
                                     ten_signature_t value);

TEN_UTILS_API ten_signature_t
ten_signature_get(const ten_signature_t *signature);
