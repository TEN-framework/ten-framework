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
// Note: To achieve the best compatibility, any new enum item, should be added
// to the end to avoid changing the value of previous enum items.
typedef enum TEN_ERROR_CODE {
  // Generic errno.
  TEN_ERROR_CODE_GENERIC = 1,

  // Invalid json.
  TEN_ERROR_CODE_INVALID_JSON = 2,

  // Invalid argument.
  TEN_ERROR_CODE_INVALID_ARGUMENT = 3,

  // Invalid type.
  TEN_ERROR_CODE_INVALID_TYPE = 4,

  // Invalid graph.
  TEN_ERROR_CODE_INVALID_GRAPH = 5,

  // The TEN world is closed.
  TEN_ERROR_CODE_TEN_IS_CLOSED = 6,

  // The msg is not connected in the graph.
  TEN_ERROR_CODE_MSG_NOT_CONNECTED = 7,
} TEN_ERROR_CODE;

static_assert(
    sizeof(TEN_ERROR_CODE) <= sizeof(ten_error_code_t),
    "The size of field TEN_ERROR_CODE enum should be less or equal to the size "
    "of ten_error_code_t.");
