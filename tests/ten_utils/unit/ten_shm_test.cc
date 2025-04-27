//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/shm.h"

TEST(ShmTest, positive) {
  // TODO(Wei): should wrapped these apis with platform target in header files
  // when not suitable for all platforms
#if defined(OS_MACOS) || defined(OS_LINUX) || defined(OS_WINDOWS)

  auto *shm1 = static_cast<ten_atomic_t *>(ten_shm_map("hello", 8));
  EXPECT_NE(shm1, nullptr);
  EXPECT_EQ(ten_shm_get_size(static_cast<void *>(shm1)), 8);
  ten_atomic_store(shm1, 0x0000000000000077);
  auto *shm2 = static_cast<ten_atomic_t *>(ten_shm_map("hello", 16));
  EXPECT_NE(shm2, nullptr);
  EXPECT_EQ(ten_atomic_load(shm2), 0x0000000000000077);
  EXPECT_EQ(ten_shm_get_size(static_cast<void *>(shm2)), 8);
  ten_shm_unmap(shm1);
  ten_atomic_store(shm2, 0x0000000000000088);
  auto *shm3 = static_cast<ten_atomic_t *>(ten_shm_map("hello", 8));
  EXPECT_NE(shm3, nullptr);
  EXPECT_EQ(ten_atomic_load(shm3), 0x0000000000000088);
  auto *shm4 = static_cast<ten_atomic_t *>(ten_shm_map("hi", 8));
  EXPECT_NE(shm4, nullptr);
  EXPECT_NE(ten_atomic_load(shm4), 0x0000000000000088);
  ten_shm_unmap(shm4);
  ten_shm_unmap(shm3);
  ten_shm_unmap(shm2);
  ten_shm_unlink("/hello");
  ten_shm_unlink("/hi");

#endif
}
