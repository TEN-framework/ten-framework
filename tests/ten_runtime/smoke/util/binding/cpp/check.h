//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_config.h"

#include <assert.h>

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "ten_runtime/binding/cpp/detail/msg/cmd_result.h"
#include "ten_runtime/common/status_code.h"

namespace ten_test {

static inline void _assert_json_equals(const nlohmann::json &actual,
                                       const std::string &expected) {
  // CAUTION: we can not use 'EXPECT_EQ(str1, str2)' here, there will be too
  // many useless logs if assertion failed.

  bool is_equals = false;

  if (actual.is_string()) {
    is_equals = (actual.get<std::string>() == expected);
  } else {
    // return_json returns an object value.
    is_equals = (actual == nlohmann::json::parse(expected));
  }

  EXPECT_EQ(true, is_equals) << "Assertion failed, expected: " << expected
                             << ", actual: " << actual << std::endl;
}

static inline void check_status_code(
    const std::unique_ptr<ten::cmd_result_t> &cmd_result,
    const TEN_STATUS_CODE status_code) {
  EXPECT_EQ(cmd_result->get_status_code(), status_code);
}

static inline void check_detail_with_json(
    const std::unique_ptr<ten::cmd_result_t> &cmd_result,
    const nlohmann::json &expected) {
  _assert_json_equals(
      nlohmann::json::parse(cmd_result->get_property_to_json("detail")),
      expected);
}

static inline void check_detail_with_string(
    const std::unique_ptr<ten::cmd_result_t> &cmd_result,
    const std::string &detail) {
  EXPECT_EQ(cmd_result->get_property_string("detail"), detail);
}

}  // namespace ten_test
