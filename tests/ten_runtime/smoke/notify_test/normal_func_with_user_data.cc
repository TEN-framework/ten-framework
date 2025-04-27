//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

void extension_on_notify(ten::ten_env_t &ten_env, void *user_data);

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void outer_thread_main(ten::ten_env_proxy_t *ten_env_proxy) {
    while (true) {
      if (trigger) {
        auto *user_data = new int64_t;
        *user_data = 3;

        ten_env_proxy->notify(extension_on_notify, user_data);
        delete ten_env_proxy;
        break;
      }

      ten_random_sleep_range_ms(0, 100);
    }
  }

  void on_start(ten::ten_env_t &ten_env) override {
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    // Create a C++ thread to call ten.xxx in it.
    outer_thread = new std::thread(&test_extension::outer_thread_main, this,
                                   ten_env_proxy);

    ten_env.on_start_done();
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    // Reclaim the C++ thread.
    outer_thread->join();
    delete outer_thread;

    ten_env.on_stop_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      // Trigger the C++ thread to call ten.xxx function.
      trigger = true;

      hello_world_cmd = std::move(cmd);
      return;
    }
  }

 private:
  friend void extension_on_notify(ten::ten_env_t &ten_env, void *user_data);

  std::thread *outer_thread{nullptr};
  std::atomic<bool> trigger{false};
  std::unique_ptr<ten::cmd_t> hello_world_cmd;
};

void extension_on_notify(ten::ten_env_t &ten_env, void *user_data) {
  auto *int64_data = static_cast<int64_t *>(user_data);

  EXPECT_EQ(*int64_data, 3);
  delete int64_data;

  auto *ext = static_cast<test_extension *>(
      ten::ten_env_internal_accessor_t::get_attached_target(ten_env));
  auto cmd_result =
      ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *ext->hello_world_cmd);
  cmd_result->set_property("detail", "hello world, too");
  ten_env.return_result(std::move(cmd_result));
}

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
    notify_test_normal_func_with_user_data__test_extension, test_extension);

}  // namespace

TEST(NotifyTest, NormalFuncWithUserData) {  // NOLINT
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
               "name": "test_extension",
               "addon": "notify_test_normal_func_with_user_data__test_extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "basic_extension_group"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "basic_extension_group", "test_extension");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world, too");

  delete client;

  ten_thread_join(app_thread, -1);
}
