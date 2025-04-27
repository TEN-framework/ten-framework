//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>

#include "ten_utils/container/hash_handle.h"
#include "ten_utils/lib/string.h"

typedef struct ten_extension_t ten_extension_t;

typedef struct ten_extension_output_msg_not_connected_count_t {
  ten_hashhandle_t hh_in_map;

  ten_string_t msg_name;
  size_t count;
} ten_extension_output_msg_not_connected_count_t;

TEN_RUNTIME_PRIVATE_API bool ten_extension_increment_msg_not_connected_count(
    ten_extension_t *extension, const char *msg_name);
