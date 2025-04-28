//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"

TEST(MsgPropertyTest, Invalid) {  // NOLINT
  auto cmd = ten::cmd_t::create("test");

  auto result = cmd->set_property("b", static_cast<void *>(nullptr));
  EXPECT_FALSE(result) << "Pointer property must not be nullptr.\n";

  result = cmd->set_property("c", static_cast<const char *>(nullptr));
  EXPECT_FALSE(result) << "String property must not be nullptr.\n";

  result = cmd->set_property("d", ten::buf_t{});
  EXPECT_FALSE(result) << "Buffer property must not be empty.\n";
}
