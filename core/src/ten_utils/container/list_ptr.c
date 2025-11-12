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

ten_listnode_t *ten_list_find_ptr(ten_list_t *self, const void *ptr) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (ten_listnode_to_ptr_listnode(iter.node)->ptr == ptr) {
      return iter.node;
    }
  }
  return NULL;
}

ten_listnode_t *ten_list_find_ptr_custom_(ten_list_t *self, const void *ptr,
                                          bool (*equal_to)(const void *,
                                                           const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr && equal_to,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (equal_to(ten_listnode_to_ptr_listnode(iter.node)->ptr, ptr)) {
      return iter.node;
    }
  }
  return NULL;
}

size_t ten_list_find_ptr_cnt_custom_(ten_list_t *self, const void *ptr,
                                     bool (*equal_to)(const void *,
                                                      const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr && equal_to,
             "Invalid argument.");

  size_t cnt = 0;
  ten_list_foreach (self, iter) {
    if (equal_to(ten_listnode_to_ptr_listnode(iter.node)->ptr, ptr)) {
      ++cnt;
    }
  }
  return cnt;
}

size_t ten_list_cnt_ptr_custom_(ten_list_t *self,
                                bool (*predicate)(const void *)) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && predicate,
             "Invalid argument.");

  size_t cnt = 0;
  ten_list_foreach (self, iter) {
    if (predicate(ten_listnode_to_ptr_listnode(iter.node)->ptr)) {
      ++cnt;
    }
  }
  return cnt;
}

bool ten_list_remove_ptr(ten_list_t *self, void *ptr) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && ptr,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (ten_listnode_to_ptr_listnode(iter.node)->ptr == ptr) {
      ten_list_remove_node(self, iter.node);
      return true;
    }
  }
  return false;
}

void ten_list_push_ptr_back(ten_list_t *self, void *ptr,
                            ten_ptr_listnode_destroy_func_t destroy) {
  TEN_ASSERT(self && ptr, "Invalid argument.");
  ten_listnode_t *listnode = ten_ptr_listnode_create(ptr, destroy);
  ten_list_push_back(self, listnode);
}

void ten_list_push_ptr_front(ten_list_t *self, void *ptr,
                             ten_ptr_listnode_destroy_func_t destroy) {
  TEN_ASSERT(self && ptr, "Invalid argument.");
  ten_listnode_t *listnode = ten_ptr_listnode_create(ptr, destroy);
  ten_list_push_front(self, listnode);
}
