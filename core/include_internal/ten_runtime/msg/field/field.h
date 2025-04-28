//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

typedef enum TEN_MSG_FIELD {
  // 'type' should be the first, because the handling method of some fields
  // would depend on the 'type'.
  TEN_MSG_FIELD_TYPE,

  TEN_MSG_FIELD_NAME,

  TEN_MSG_FIELD_SRC,
  TEN_MSG_FIELD_DEST,
  TEN_MSG_FIELD_PROPERTIES,

  TEN_MSG_FIELD_LAST,
} TEN_MSG_FIELD;
