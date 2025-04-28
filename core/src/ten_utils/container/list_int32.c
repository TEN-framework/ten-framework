//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/macro/check.h"

ten_listnode_t *ten_list_find_int32(ten_list_t *self, int32_t int32) {
  TEN_ASSERT(self && ten_list_check_integrity(self), "Invalid argument.");

  ten_list_foreach (self, iter) {
    int32_t self_value = ten_listnode_to_int32_listnode(iter.node)->int32;
    if (self_value == int32) {
      return iter.node;
    }
  }
  return NULL;
}
