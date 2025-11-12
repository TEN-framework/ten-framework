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
#include "ten_utils/container/list_node_smart_ptr.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

ten_listnode_t *ten_list_find_shared_ptr_custom_(
    ten_list_t *self, const void *ptr,
    bool (*equal_to)(const void *, const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && equal_to,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (equal_to(ten_shared_ptr_get_data(
                     ten_listnode_to_smart_ptr_listnode(iter.node)->ptr),
                 ptr)) {
      return iter.node;
    }
  }
  return NULL;
}

ten_listnode_t *ten_list_find_shared_ptr_custom_2_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2,
    bool (*equal_to)(const void *, const void *, const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && equal_to,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (equal_to(ten_shared_ptr_get_data(
                     ten_listnode_to_smart_ptr_listnode(iter.node)->ptr),
                 ptr_1, ptr_2)) {
      return iter.node;
    }
  }
  return NULL;
}

ten_listnode_t *ten_list_find_shared_ptr_custom_3_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2, const void *ptr_3,
    bool (*equal_to)(const void *, const void *, const void *, const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && equal_to,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (equal_to(ten_shared_ptr_get_data(
                     ten_listnode_to_smart_ptr_listnode(iter.node)->ptr),
                 ptr_1, ptr_2, ptr_3)) {
      return iter.node;
    }
  }
  return NULL;
}

ten_listnode_t *ten_list_find_shared_ptr_custom_4_(
    ten_list_t *self, const void *ptr_1, const void *ptr_2, const void *ptr_3,
    const void *ptr_4,
    bool (*equal_to)(const void *, const void *, const void *, const void *,
                     const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && equal_to,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (equal_to(ten_shared_ptr_get_data(
                     ten_listnode_to_smart_ptr_listnode(iter.node)->ptr),
                 ptr_1, ptr_2, ptr_3, ptr_4)) {
      return iter.node;
    }
  }
  return NULL;
}

ten_smart_ptr_t *ten_list_push_smart_ptr_back(ten_list_t *self,
                                              ten_smart_ptr_t *ptr) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr,
             "Invalid argument.");

  ten_listnode_t *listnode = ten_smart_ptr_listnode_create(ptr);
  ten_list_push_back(self, listnode);
  return ((ten_smart_ptr_listnode_t *)listnode)->ptr;
}

static bool ten_list_ptr_equal_to(const void *ptr_in_list,
                                  const void *raw_ptr) {
  TEN_ASSERT(ptr_in_list && raw_ptr, "Invalid argument.");

  return ptr_in_list == raw_ptr;
}

ten_listnode_t *ten_list_find_shared_ptr(ten_list_t *self, const void *ptr) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr,
             "Invalid argument.");

  return ten_list_find_shared_ptr_custom_(self, ptr, ten_list_ptr_equal_to);
}
