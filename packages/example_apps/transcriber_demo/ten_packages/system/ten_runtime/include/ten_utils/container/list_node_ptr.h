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

#define TEN_NORMAL_PTR_LISTNODE_SIGNATURE 0xEBB1285007CA4A12U

typedef void (*ten_ptr_listnode_destroy_func_t)(void *ptr);

typedef struct ten_ptr_listnode_t {
  ten_listnode_t hdr;
  ten_signature_t signature;
  void *ptr;
  ten_ptr_listnode_destroy_func_t destroy;
} ten_ptr_listnode_t;

TEN_UTILS_API ten_listnode_t *ten_ptr_listnode_create(
    void *ptr, ten_ptr_listnode_destroy_func_t destroy);

TEN_UTILS_API ten_ptr_listnode_t *ten_listnode_to_ptr_listnode(
    ten_listnode_t *self);

TEN_UTILS_API ten_listnode_t *ten_listnode_from_ptr_listnode(
    ten_ptr_listnode_t *self);

TEN_UTILS_API void *ten_ptr_listnode_get(ten_listnode_t *self);

TEN_UTILS_API void ten_ptr_listnode_replace(
    ten_listnode_t *self, void *ptr, ten_ptr_listnode_destroy_func_t destroy);
