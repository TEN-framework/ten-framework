//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <string>

#include "gtest/gtest.h"
#include "ten_utils/lib/string.h"

TEST(StringTest, equal) {
  const std::string test_str1 = "testing string";
  const std::string test_str2 = "testing String";

  auto s = ten_string_create_formatted(test_str1.c_str());
  auto r = ten_string_is_equal_c_str(s, test_str1.c_str());
  EXPECT_EQ(r, true);

  r = ten_string_is_equal_c_str(s, test_str2.c_str());
  EXPECT_EQ(r, false);

  r = ten_string_is_equal_c_str_case_insensitive(s, test_str2.c_str());
  EXPECT_EQ(r, true);

  ten_string_destroy(s);
}

TEST(StringTest, concat) {
  const std::string test_str1 = "testing string";
  const std::string test_str2 = "testing String";

  auto s = ten_string_create_formatted(test_str1.c_str());

  ten_string_append_formatted(s, "%.*s", test_str2.length(), test_str2.c_str());
  auto r = ten_string_is_equal_c_str(s, (test_str1 + test_str2).c_str());
  EXPECT_EQ(r, true);

  ten_string_destroy(s);
}
