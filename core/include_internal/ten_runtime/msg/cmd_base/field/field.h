//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

typedef enum TEN_CMD_BASE_FIELD {
  TEN_CMD_BASE_FIELD_MSGHDR,

  TEN_CMD_BASE_FIELD_CMD_ID,
  TEN_CMD_BASE_FIELD_SEQ_ID,

  TEN_CMD_BASE_FIELD_RESPONSE_HANDLER,
  TEN_CMD_BASE_FIELD_RESPONSE_HANDLER_DATA,

  TEN_CMD_BASE_FIELD_LAST,
} TEN_CMD_BASE_FIELD;
