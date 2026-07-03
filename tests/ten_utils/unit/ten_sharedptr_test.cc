//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <string>

#include "gtest/gtest.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"

#define BUF_SIZE 100

void destroy(void *m) { ((char *)m)[BUF_SIZE] = 0; }

TEST(SharedptrTest, positive) {
  char *m = (char *)ten_malloc(BUF_SIZE + 1);
  memset(m, 0, BUF_SIZE);
  m[BUF_SIZE] = 3;

  ten_shared_ptr_t *p1 = ten_shared_ptr_create(m, destroy);

  ten_shared_ptr_t *p2 = ten_shared_ptr_clone(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_t *p3 = ten_shared_ptr_clone(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p2);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p3);
  EXPECT_EQ(m[BUF_SIZE], 0);

  ten_free(m);
}

TEST(SharedptrTest, weakptr) {
  char *m = (char *)ten_malloc(BUF_SIZE + 1);
  memset(m, 0, BUF_SIZE);
  m[BUF_SIZE] = 3;

  ten_shared_ptr_t *p1 = ten_shared_ptr_create(m, destroy);

  ten_shared_ptr_t *p2 = ten_shared_ptr_clone(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_weak_ptr_t *w1 = ten_weak_ptr_create(p1);

  ten_weak_ptr_t *w2 = ten_weak_ptr_clone(w1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_weak_ptr_destroy(w1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p2);
  EXPECT_EQ(m[BUF_SIZE], 0);

  ten_weak_ptr_destroy(w2);
  EXPECT_EQ(m[BUF_SIZE], 0);

  ten_free(m);
}

TEST(SharedptrTest, weakptr_lock) {
  char *m = (char *)ten_malloc(BUF_SIZE + 1);
  memset(m, 0, BUF_SIZE);
  m[BUF_SIZE] = 3;

  ten_shared_ptr_t *p1 = ten_shared_ptr_create(m, destroy);

  ten_shared_ptr_t *p2 = ten_shared_ptr_clone(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_weak_ptr_t *w1 = ten_weak_ptr_create(p1);

  ten_weak_ptr_t *w2 = ten_weak_ptr_clone(w1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_weak_ptr_destroy(w2);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_t *l1 = ten_weak_ptr_lock(w1);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(p2);
  EXPECT_EQ(m[BUF_SIZE], 3);

  ten_shared_ptr_destroy(l1);
  EXPECT_EQ(m[BUF_SIZE], 0);

  ten_weak_ptr_destroy(w1);
  EXPECT_EQ(m[BUF_SIZE], 0);

  ten_free(m);
}
