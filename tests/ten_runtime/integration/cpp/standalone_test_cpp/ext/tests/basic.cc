//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "ten_runtime/binding/cpp/ten.h"

namespace {

class extension_tester_1 : public ten::extension_tester_t {
 public:
  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Send the first command to the extension.
    auto new_cmd = ten::cmd_t::create("hello_world");

    ten_env.send_cmd(
        std::move(new_cmd), [](ten::ten_env_tester_t &ten_env,
                               std::unique_ptr<ten::cmd_result_t> cmd_result,
                               ten::error_t * /*error*/) {
          if (cmd_result->get_status_code() == TEN_STATUS_CODE_OK) {
            ten_env.stop_test();
          }
        });

    ten_env.on_start_done();
  }
};

class extension_tester_2 : public ten::extension_tester_t {
 public:
  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Send the first command to the extension.
    auto new_cmd = ten::cmd_t::create("aaa");

    ten_env.send_cmd(
        std::move(new_cmd), [](ten::ten_env_tester_t &ten_env,
                               std::unique_ptr<ten::cmd_result_t> cmd_result,
                               ten::error_t * /*error*/) {
          if (cmd_result->get_status_code() == TEN_STATUS_CODE_ERROR) {
            ten_env.stop_test();
          }
        });

    ten_env.on_start_done();
  }
};

}  // namespace

TEST(Test, Basic) {  // NOLINT
  auto *tester = new extension_tester_1();
  tester->set_test_mode_single("ext");
  tester->run();
  delete tester;
}

TEST(Test, Basic2) {  // NOLINT
  auto *tester = new extension_tester_2();
  tester->set_test_mode_single("ext");
  tester->run();
  delete tester;
}
