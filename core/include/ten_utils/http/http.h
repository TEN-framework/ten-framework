//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

typedef enum TEN_HTTP_METHOD {
  TEN_HTTP_METHOD_INVALID,

  TEN_HTTP_METHOD_GET,
  TEN_HTTP_METHOD_POST,
  TEN_HTTP_METHOD_PUT,
  TEN_HTTP_METHOD_PATCH,
  TEN_HTTP_METHOD_DELETE,
  TEN_HTTP_METHOD_HEAD,
  TEN_HTTP_METHOD_OPTIONS,

  TEN_HTTP_METHOD_FIRST = TEN_HTTP_METHOD_GET,
  TEN_HTTP_METHOD_LAST = TEN_HTTP_METHOD_OPTIONS,
} TEN_HTTP_METHOD;
