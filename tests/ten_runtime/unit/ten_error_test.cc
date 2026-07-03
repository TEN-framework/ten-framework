//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <thread>

#include "gtest/gtest.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"

static void cpp_thread() {
  ten_error_t *err = ten_error_create();

  // Not set before, errno is TEN_ERROR_CODE_OK.
  EXPECT_EQ(ten_error_code(err), TEN_ERROR_CODE_OK);

  ten_error_set(err, 1, "Error msg in cpp_thread.");
  EXPECT_EQ(ten_error_code(err), 1);
  EXPECT_STREQ(ten_error_message(err), "Error msg in cpp_thread.");

  ten_error_destroy(err);
}

TEST(TenErrorTest, cpp_thread) {
  ten_error_t *outter_error = ten_error_create();
  ten_error_set(outter_error, TEN_ERROR_CODE_INVALID_GRAPH,
                "Incorrect graph definition");

  std::thread t1(cpp_thread);

  EXPECT_STREQ(ten_error_message(outter_error), "Incorrect graph definition");

  t1.join();
  ten_error_destroy(outter_error);
}
