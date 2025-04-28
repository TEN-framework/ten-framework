//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "nlohmann/json_fwd.hpp"
#include "ten_runtime/binding/cpp/detail/extension.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

// This part is the extension codes written by the developer, maintained in its
// final release form, and will not change due to testing requirements.

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      auto greeting_words = ten_env.get_property_string("greeting_words");

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", greeting_words);
      bool rc = ten_env.return_result(std::move(cmd_result));
      EXPECT_EQ(rc, true);
    }
  }
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    standalone_test_set_property_for_single_ext__test_extension_1,
    test_extension_1);

}  // namespace

namespace {

class extension_tester_1 : public ten::extension_tester_t {
 public:
  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Send the first command to the extension.
    auto new_cmd = ten::cmd_t::create("hello_world");

    ten_env.send_cmd(
        std::move(new_cmd),
        [](ten::ten_env_tester_t &ten_env,
           std::unique_ptr<ten::cmd_result_t> result, ten::error_t *err) {
          if (result->get_status_code() == TEN_STATUS_CODE_OK) {
            auto detail = result->get_property_string("detail");
            EXPECT_EQ(detail, "nice to meet you");
            ten_env.stop_test();
          }
        });

    ten_env.on_start_done();
  }
};

}  // namespace

TEST(StandaloneTest, SetPropertyForSingleExt) {  // NOLINT
  auto *tester = new extension_tester_1();
  tester->set_test_mode_single(
      "standalone_test_set_property_for_single_ext__test_extension_1",
      R"({
        "greeting_words": "nice to meet you"
      })");

  bool rc = tester->run();
  TEN_ASSERT(rc, "Should not happen.");

  delete tester;
}
