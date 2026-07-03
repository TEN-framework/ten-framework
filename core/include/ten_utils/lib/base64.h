//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/string.h"

TEN_UTILS_API bool ten_base64_to_string(ten_string_t *result, ten_buf_t *buf);

TEN_UTILS_API bool ten_base64_from_string(ten_string_t *str, ten_buf_t *result);
