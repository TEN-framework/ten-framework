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

#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/signature.h"

#define TEN_INT32_LISTNODE_SIGNATURE 0x2A576F8836859FB5U

typedef struct ten_int32_listnode_t {
  ten_listnode_t hdr;
  ten_signature_t signature;
  int32_t int32;
} ten_int32_listnode_t;

TEN_UTILS_API ten_listnode_t *ten_int32_listnode_create(int32_t int32);

TEN_UTILS_API ten_int32_listnode_t *ten_listnode_to_int32_listnode(
    ten_listnode_t *self);

TEN_UTILS_API ten_listnode_t *ten_listnode_from_int32_listnode(
    ten_int32_listnode_t *self);

TEN_UTILS_API int32_t ten_int32_listnode_get(ten_listnode_t *self);
