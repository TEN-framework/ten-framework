//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/error.h"

// Define various error numbers here.
//
// Note: The TEN_ERROR_CODE_INTERNAL here is not a public API of TEN; it is only
// used internally within TEN. Start with a very large value, such as 100,000,
// to avoid conflicts with the current and future `TEN_ERROR_CODE` enum values.
typedef enum TEN_ERROR_CODE_INTERNAL {
  // Specified property does not exist.
  TEN_ERROR_CODE_VALUE_NOT_FOUND = 100000,
} TEN_ERROR_CODE_INTERNAL;

static_assert(
    sizeof(TEN_ERROR_CODE_INTERNAL) <= sizeof(ten_error_code_t),
    "The size of field TEN_ERROR_CODE enum should be less or equal to the size "
    "of ten_error_code_t.");
