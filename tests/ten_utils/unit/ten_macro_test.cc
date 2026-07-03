//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "common/test_utils.h"
#include "gtest/gtest.h"
#include "ten_utils/macro/expand.h"

TEST(MacroTest, positive) {
#if defined(__clang__)
  // The following line could only be passed if this file is compiled with
  // clang.
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(), 0);
#endif

  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(a), 1);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(a, 1), 2);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(a, b, c), 3);
  EXPECT_EQ(TEN_MACRO_CAR(1, 2, 3), 1);
  EXPECT_EQ(TEN_MACRO_CAR("a", "b", "c"), "a");
  EXPECT_EQ(TEN_MACRO_CAR(nullptr, "b", 3), nullptr);
  EXPECT_EQ(TEN_MACRO_CAR(TEN_MACRO_CAR(1, 2, 3)), 1);
  EXPECT_EQ(TEN_MACRO_CAR(TEN_MACRO_CDR(1, 2, 3)), 2);
  EXPECT_EQ(TEN_MACRO_CDR(TEN_MACRO_CDR(1, 2, 3)), 3);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(TEN_MACRO_CAR(1, 2, 3)), 1);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(TEN_MACRO_CDR(1, 2, 3)), 2);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(TEN_MACRO_CDR(1, 2)), 1);
  EXPECT_EQ(TEN_MACRO_ARGS_COUNT(TEN_MACRO_CDR(1)), 1);
  TEN_MACRO_REPEAT(5, AGO_LOG("hello"); AGO_LOG("world"););
  // TEN_MACRO_REPEAT_WITH_INDEX(1, AGO_LOG("hello" #times);)
}