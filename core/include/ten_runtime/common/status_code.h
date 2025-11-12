//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

typedef enum TEN_STATUS_CODE {
  TEN_STATUS_CODE_INVALID = -1,

  // 0 representing OK is a common convention.
  TEN_STATUS_CODE_OK = 0,
  TEN_STATUS_CODE_ERROR = 1,

  TEN_STATUS_CODE_LAST = 2,
} TEN_STATUS_CODE;
