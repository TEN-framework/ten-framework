//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/string.h"

#define TEN_PROTOCOL_TCP "tcp"
#define TEN_PROTOCOL_RAW "raw"
#define TEN_PROTOCOL_PIPE "pipe"

typedef struct ten_value_t ten_value_t;

TEN_UTILS_API ten_string_t *ten_uri_get_protocol(const char *uri);

TEN_UTILS_API bool ten_uri_is_protocol_equal(const char *uri,
                                             const char *protocol);

TEN_UTILS_API ten_string_t *ten_uri_get_host(const char *uri);

TEN_UTILS_API uint16_t ten_uri_get_port(const char *uri);
