//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "ten_utils/container/list.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value_kv.h"

#define TEN_VALUE_KV_SIGNATURE 0xCF7DC27C3B187517U

TEN_UTILS_API ten_value_kv_t *ten_value_kv_create_vempty(const char *fmt, ...);
