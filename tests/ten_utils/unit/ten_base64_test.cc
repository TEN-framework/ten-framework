//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stdint.h>

#include "gtest/gtest.h"
#include "ten_utils/lib/base64.h"
#include "ten_utils/lib/buf.h"

TEST(Base64Test, positive) {
  ten_string_t result;
  TEN_STRING_INIT(result);

  const char *src_str = "how_are_you_this_morning";
  ten_buf_t src_str_buf = TEN_BUF_STATIC_INIT_WITH_DATA_UNOWNED(
      (uint8_t *)src_str, strlen(src_str));
  bool rc = ten_base64_to_string(&result, &src_str_buf);
  EXPECT_EQ(rc, true);
  EXPECT_EQ(
      ten_string_is_equal_c_str(&result, "aG93X2FyZV95b3VfdGhpc19tb3JuaW5n"),
      true);

  ten_buf_deinit(&src_str_buf);

  ten_buf_t convert_back_data = TEN_BUF_STATIC_INIT_OWNED;
  rc = ten_base64_from_string(&result, &convert_back_data);
  EXPECT_EQ(rc, true);
  EXPECT_NE(convert_back_data.content_size, 0);
  EXPECT_EQ(ten_c_string_is_equal_with_size(
                src_str, (const char *)convert_back_data.data,
                convert_back_data.content_size),
            true);

  ten_buf_deinit(&convert_back_data);
  ten_string_deinit(&result);
}
