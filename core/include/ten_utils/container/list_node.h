//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"

#define TEN_LISTNODE_SIGNATURE 0x00D7B10E642B105CU

typedef struct ten_listnode_t ten_listnode_t;
typedef struct ten_list_t ten_list_t;

struct ten_listnode_t {
  ten_signature_t signature;
  ten_listnode_t *next, *prev;
  void (*destroy)(ten_listnode_t *);
};

#include "ten_utils/container/list_node_int32.h"      // IWYU pragma: keep
#include "ten_utils/container/list_node_ptr.h"        // IWYU pragma: keep
#include "ten_utils/container/list_node_smart_ptr.h"  // IWYU pragma: keep
#include "ten_utils/container/list_node_str.h"        // IWYU pragma: keep

TEN_UTILS_API bool ten_listnode_check_integrity(ten_listnode_t *self);

TEN_UTILS_PRIVATE_API void ten_listnode_init(ten_listnode_t *self,
                                             void *destroy);

TEN_UTILS_API void ten_listnode_destroy(ten_listnode_t *self);
