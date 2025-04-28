//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      // Remember the command sent from the client, so that we can send its
      // result back to the client.
      client_cmd = std::move(cmd);

      auto hello_world_cmd = ten::cmd_t::create("hello_world");
      ten_env.send_cmd(
          std::move(hello_world_cmd),
          [&](ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_result_t> cmd_result,
              ten::error_t *err) {
            // Return to the client to notify that the whole process
            // is complete successfully.
            auto cmd_result_for_hello_world =
                ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *client_cmd);
            cmd_result_for_hello_world->set_property("detail", "OK");
            ten_env.return_result(std::move(cmd_result_for_hello_world));
          });

      return;
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> client_cmd;
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Extension 2 is just a forwarding proxy, forward all the commands it
    // received out.

    if (cmd->get_name() == "hello_world") {
      ten_env.send_cmd(std::move(cmd));
      return;
    }
  }
};

class test_extension_3 : public ten::extension_t {
 public:
  explicit test_extension_3(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Do not destroy the channel.
    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property("detail", "hello world from extension 3, too");
    ten_env.return_result(std::move(cmd_result));
  }
};

class test_extension_4 : public ten::extension_t {
 public:
  explicit test_extension_4(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Do not destroy the channel.
    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property("detail", "hello world from extension 4, too");
    ten_env.return_result(std::move(cmd_result));
  }
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(multi_dest_y_graph__extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(multi_dest_y_graph__extension_2,
                                    test_extension_2);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(multi_dest_y_graph__extension_3,
                                    test_extension_3);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(multi_dest_y_graph__extension_4,
                                    test_extension_4);

}  // namespace

TEST(MultiDestTest, MultiDestYGraph) {  // NOLINT
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
               "name": "extension_1",
               "addon": "multi_dest_y_graph__extension_1",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_group"
             },{
               "type": "extension",
               "name": "extension_2",
               "addon": "multi_dest_y_graph__extension_2",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_group"
             },{
               "type": "extension",
               "name": "extension_3",
               "addon": "multi_dest_y_graph__extension_3",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_group"
             },{
               "type": "extension",
               "name": "extension_4",
               "addon": "multi_dest_y_graph__extension_4",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_group"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "extension_1",
               "cmd": [{
                 "name": "hello_world",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "extension_2"
                 }]
               }]
             },{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "extension_2",
               "cmd": [{
                 "name": "hello_world",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "extension_3"
                 },{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "extension_4"
                 }]
               }]
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "extension_group", "extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "OK");

  delete client;

  ten_thread_join(app_thread, -1);
}
