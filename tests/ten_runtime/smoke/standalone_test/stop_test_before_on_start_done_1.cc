//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/extension.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/time.h"
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
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      bool rc = ten_env.return_result(std::move(cmd_result));
      EXPECT_EQ(rc, true);
    }
  }
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    stop_test_before_on_start_done_1__test_extension_1, test_extension_1);

}  // namespace

namespace {

class extension_tester_1 : public ten::extension_tester_t {
 public:
  extension_tester_1() = default;

  ~extension_tester_1() override {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // @{
  extension_tester_1(const extension_tester_1 &) = delete;
  extension_tester_1(extension_tester_1 &&) = delete;
  extension_tester_1 &operator=(const extension_tester_1 &) = delete;
  extension_tester_1 &operator=(const extension_tester_1 &&) = delete;
  // @}

  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Send the first command to the extension.
    auto new_cmd = ten::cmd_t::create("hello_world");

    ten_env.send_cmd(
        std::move(new_cmd),
        [this](ten::ten_env_tester_t &ten_env,
               std::unique_ptr<ten::cmd_result_t> result, ten::error_t *err) {
          TEN_ASSERT(result->get_status_code() == TEN_STATUS_CODE_OK,
                     "Should not happen.");

          ten_env.stop_test();

          // Create ten_env_proxy to be used in the other thread.
          auto *ten_env_proxy = ten::ten_env_tester_proxy_t::create(ten_env);

          // Create an other thread to test if the runtime can handle the
          // situation where stop_test() is called before on_start_done.
          thread_ = std::thread([ten_env_proxy]() {
            ten_sleep_ms(1000);

            ten_env_proxy->notify([](ten::ten_env_tester_t &ten_env) {
              ten_env.on_start_done();
            });

            delete ten_env_proxy;
          });
        });
  }

 private:
  std::thread thread_;
};

}  // namespace

TEST(StandaloneTest, StopTestBeforeOnStartDone1) {  // NOLINT
  auto *tester = new extension_tester_1();
  tester->set_test_mode_single(
      "stop_test_before_on_start_done_1__test_extension_1");

  bool rc = tester->run();
  TEN_ASSERT(rc, "Should not happen.");

  delete tester;
}
