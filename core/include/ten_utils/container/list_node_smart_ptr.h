//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"

#define TEN_SMART_PTR_LISTNODE_SIGNATURE 0x00C0ADEEF6B9A421U

typedef struct ten_smart_ptr_listnode_t {
  ten_listnode_t hdr;
  ten_signature_t signature;
  ten_smart_ptr_t *ptr;
} ten_smart_ptr_listnode_t;

TEN_UTILS_API ten_listnode_t *ten_smart_ptr_listnode_create(
    ten_smart_ptr_t *ptr);

TEN_UTILS_API ten_smart_ptr_listnode_t *ten_listnode_to_smart_ptr_listnode(
    ten_listnode_t *self);

TEN_UTILS_API ten_listnode_t *ten_listnode_from_smart_ptr_listnode(
    ten_smart_ptr_listnode_t *self);

TEN_UTILS_API ten_smart_ptr_t *ten_smart_ptr_listnode_get(ten_listnode_t *self);
