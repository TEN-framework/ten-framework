//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstdint>
#include <iostream>

#include "common/test_utils.h"
#include "gtest/gtest.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"

namespace {

int compare_int32(ten_listnode_t *x, ten_listnode_t *y) {
  ten_int32_listnode_t *ix = ten_listnode_to_int32_listnode(x);
  ten_int32_listnode_t *iy = ten_listnode_to_int32_listnode(y);

  return iy->int32 - ix->int32;
}

}  // namespace

TEST(TenListTest, OrderedInsert) {  // NOLINT
  // Insert into an empty list
  ten_list_t list = TEN_LIST_INIT_VAL;

  ten_listnode_t *one = ten_int32_listnode_create(1);
  ten_list_push_back_in_order(&list, one, compare_int32, false);

  EXPECT_EQ(ten_list_size(&list), 1);

  ten_int32_listnode_t *front =
      ten_listnode_to_int32_listnode(ten_list_front(&list));
  EXPECT_EQ(front->int32, 1);

  ten_int32_listnode_t *back =
      ten_listnode_to_int32_listnode(ten_list_back(&list));
  EXPECT_EQ(back->int32, 1);

  // Insert front
  ten_listnode_t *zero = ten_int32_listnode_create(0);
  ten_list_push_back_in_order(&list, zero, compare_int32, false);

  EXPECT_EQ(ten_list_size(&list), 2);

  front = ten_listnode_to_int32_listnode(ten_list_front(&list));
  EXPECT_EQ(front->int32, 0);

  back = ten_listnode_to_int32_listnode(ten_list_back(&list));
  EXPECT_EQ(back->int32, 1);

  // Insert back
  ten_listnode_t *three = ten_int32_listnode_create(3);
  ten_list_push_back_in_order(&list, three, compare_int32, false);

  EXPECT_EQ(ten_list_size(&list), 3);

  front = ten_listnode_to_int32_listnode(ten_list_front(&list));
  EXPECT_EQ(front->int32, 0);

  back = ten_listnode_to_int32_listnode(ten_list_back(&list));
  EXPECT_EQ(back->int32, 3);

  // Insert middle
  ten_listnode_t *two = ten_int32_listnode_create(2);
  ten_list_push_back_in_order(&list, two, compare_int32, false);

  EXPECT_EQ(ten_list_size(&list), 4);

  ten_int32_listnode_t *node_two =
      ten_listnode_to_int32_listnode(ten_list_back(&list)->prev);
  EXPECT_EQ(node_two->int32, 2);

  // Insert existed value
  ten_listnode_t *another_one = ten_int32_listnode_create(1);
  ten_listnode_t *another_three = ten_int32_listnode_create(3);
  ten_list_push_back_in_order(&list, another_one, compare_int32, false);
  ten_list_push_back_in_order(&list, another_three, compare_int32, false);

  EXPECT_EQ(ten_list_size(&list), 6);
  EXPECT_EQ(ten_listnode_to_int32_listnode(ten_list_front(&list)->next)->int32,
            1);
  EXPECT_EQ(ten_listnode_to_int32_listnode(ten_list_back(&list)->prev)->int32,
            3);

  // Debug log. [0,1,1,2,3,3]
  // ten_list_foreach (&list, iter) {
  //   std::cout << ten_listnode_to_int32_listnode(iter.node)->int32 << " ";
  // }
  // std::cout << std::endl;

  ten_list_clear(&list);
}

TEST(TenListTest, IteratorNext) {  // NOLINT
  ten_list_t list = TEN_LIST_INIT_VAL;

  ten_listnode_t *one = ten_int32_listnode_create(1);
  ten_list_push_back(&list, one);

  ten_listnode_t *two = ten_int32_listnode_create(2);
  ten_list_push_back(&list, two);

  ten_listnode_t *three = ten_int32_listnode_create(3);
  ten_list_push_back(&list, three);

  ten_list_iterator_t iter = ten_list_begin(&list);
  EXPECT_EQ(1, ten_listnode_to_int32_listnode(iter.node)->int32);

  iter = ten_list_iterator_next(iter);
  EXPECT_EQ(2, ten_listnode_to_int32_listnode(iter.node)->int32);

  iter = ten_list_iterator_next(iter);
  EXPECT_EQ(3, ten_listnode_to_int32_listnode(iter.node)->int32);

  iter = ten_list_iterator_next(iter);
  EXPECT_EQ(NULL, iter.node);

  ten_list_clear(&list);
}
