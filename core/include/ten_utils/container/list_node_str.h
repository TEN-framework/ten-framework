//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

#define TEN_STR_LISTNODE_SIGNATURE 0x15D33B50C27A1B20U

typedef struct ten_str_listnode_t {
  ten_listnode_t hdr;
  ten_signature_t signature;
  ten_string_t str;
} ten_str_listnode_t;

TEN_UTILS_API ten_listnode_t *ten_str_listnode_create(const char *str);

TEN_UTILS_API ten_listnode_t *ten_str_listnode_create_with_size(const char *str,
                                                                size_t size);

TEN_UTILS_API ten_str_listnode_t *ten_listnode_to_str_listnode(
    ten_listnode_t *self);

TEN_UTILS_API ten_listnode_t *ten_listnode_from_str_listnode(
    ten_str_listnode_t *self);

TEN_UTILS_API ten_string_t *ten_str_listnode_get(ten_listnode_t *self);
