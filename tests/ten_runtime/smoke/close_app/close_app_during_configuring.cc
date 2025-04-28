//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/ten_env_proxy.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "close_app") {
      auto close_app_cmd = ten::cmd_close_app_t::create();
      close_app_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
      ten_env.send_cmd(std::move(close_app_cmd));

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "app closed");

      ten_env.return_result(std::move(cmd_result));
      return;
    }
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    // Create a ten_env_proxy to be used in the other thread.
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    // Start a thread and have it wait for 1 second to explicitly simulate the
    // `on_xxx_done` event occurring only after the close app action has been
    // triggered. The actual close app process will only take place after the
    // `on_xxx_done` event has occurred.
    thread_ = std::thread([ten_env_proxy]() {
      // Wait for some seconds to ensure that `app/graph` is closed before
      // `on_configure_done`.
      ten_random_sleep_range_ms(1000, 2000);

      ten_env_proxy->notify(
          [](ten::ten_env_t &ten_env) { ten_env.on_configure_done(); });

      delete ten_env_proxy;
    });
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    thread_.join();

    // After ensuring the other thread is joined, we could declare
    // `on_stop_done`.
    ten_env.on_stop_done();
  }

 private:
  std::thread thread_;
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8001/",
               "log": {
                 "level": 2
               }
             }
           })",
        // clang-format on
        nullptr);
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *test_app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    close_app_during_configuring__test_extension_1, test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    close_app_during_configuring__test_extension_2, test_extension_2);

}  // namespace

TEST(CloseAppTest, CloseAppDuringConfiguring) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
                "type": "extension",
                "name": "test_extension_1",
                "addon": "close_app_during_configuring__test_extension_1",
                "extension_group": "basic_extension_group_1",
                "app": "msgpack://127.0.0.1:8001/"
             },{
                "type": "extension",
                "name": "test_extension_2",
                "addon": "close_app_during_configuring__test_extension_2",
                "extension_group": "basic_extension_group_2",
                "app": "msgpack://127.0.0.1:8001/"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a close_app command.
  auto close_app_cmd = ten::cmd_t::create("close_app");
  close_app_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                          "basic_extension_group_1", "test_extension_1");
  client->send_cmd(std::move(close_app_cmd));

  ten_thread_join(app_thread, -1);

  delete client;
}
