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

void ten_list_push_str_back(ten_list_t *self, const char *str) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && str,
             "Invalid argument.");

  ten_listnode_t *listnode = ten_str_listnode_create(str);
  ten_list_push_back(self, listnode);
}

void ten_list_push_str_front(ten_list_t *self, const char *str) {
  TEN_ASSERT(self && str, "Invalid argument.");

  ten_listnode_t *listnode = ten_str_listnode_create(str);
  ten_list_push_front(self, listnode);
}

void ten_list_push_str_with_size_back(ten_list_t *self, const char *str,
                                      size_t size) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && str,
             "Invalid argument.");

  ten_listnode_t *listnode = ten_str_listnode_create_with_size(str, size);
  ten_list_push_back(self, listnode);
}

ten_listnode_t *ten_list_find_string(ten_list_t *self, const char *str) {
  TEN_ASSERT(self && ten_list_check_integrity(self) && str,
             "Invalid argument.");

  ten_list_foreach (self, iter) {
    if (ten_string_is_equal_c_str(
            &(ten_listnode_to_str_listnode(iter.node)->str), str)) {
      return iter.node;
    }
  }
  return NULL;
}
