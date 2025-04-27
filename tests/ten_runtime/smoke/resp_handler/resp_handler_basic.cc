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
    if (cmd->get_name() == "hello_world_1") {
      auto test_string = std::make_shared<std::string>("test test test");

      ten_env.send_cmd(
          std::move(cmd),
          [test_string](ten::ten_env_t &ten_env,
                        std::unique_ptr<ten::cmd_result_t> cmd_result,
                        ten::error_t *err) {
            nlohmann::json json =
                nlohmann::json::parse(cmd_result->get_property_to_json());

            if (json.value("detail", "") == "hello world 1, too" &&
                *test_string == "test test test") {
              cmd_result->set_property("detail", "hello world 1, too");
              ten_env.return_result(std::move(cmd_result));
            }
          });
    } else if (cmd->get_name() == "hello_world_2") {
      ten_env.send_cmd(
          std::move(cmd),
          [](ten::ten_env_t &ten_env,
             std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
            nlohmann::json json =
                nlohmann::json::parse(cmd_result->get_property_to_json());
            if (json.value("detail", "") == "hello world 2, too") {
              cmd_result->set_property("detail", "hello world 2, too");
              ten_env.return_result(std::move(cmd_result));
            };
          });
    } else if (cmd->get_name() == "hello_world_3") {
      ten_env.send_cmd(
          std::move(cmd),
          [](ten::ten_env_t &ten_env,
             std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
            nlohmann::json json =
                nlohmann::json::parse(cmd_result->get_property_to_json());
            if (json.value("detail", "") == "hello world 3, too") {
              cmd_result->set_property("detail", "hello world 3, too");
              ten_env.return_result(std::move(cmd_result));
            }
          });
    } else if (cmd->get_name() == "hello_world_4") {
      hello_world_4_cmd = std::move(cmd);

      auto hello_world_5_cmd = ten::cmd_t::create("hello_world_5");
      ten_env.send_cmd(
          std::move(hello_world_5_cmd),
          [&](ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_result_t> cmd_result,
              ten::error_t *err) {
            if (cmd_result->get_property_string("detail") ==
                "hello world 5, too") {
              auto cmd_result_for_hello_world = ten::cmd_result_t::create(
                  TEN_STATUS_CODE_OK, *hello_world_4_cmd);
              cmd_result_for_hello_world->set_property("detail",
                                                       "hello world 4, too");
              ten_env.return_result(std::move(cmd_result_for_hello_world));
            }
          });
    } else if (cmd->get_name() == "hello_world_5") {
      auto cmd_shared =
          std::make_shared<std::unique_ptr<ten::cmd_t>>(std::move(cmd));

      auto hello_world_6_cmd = ten::cmd_t::create("hello_world_6");
      ten_env.send_cmd(
          std::move(hello_world_6_cmd),
          [cmd_shared](ten::ten_env_t &ten_env,
                       std::unique_ptr<ten::cmd_result_t> cmd_result,
                       ten::error_t *err) {
            nlohmann::json json =
                nlohmann::json::parse(cmd_result->get_property_to_json());
            if (json.value("detail", "") == "hello world 6, too") {
              auto cmd_result =
                  ten::cmd_result_t::create(TEN_STATUS_CODE_OK, **cmd_shared);
              cmd_result->set_property("detail", "hello world 5, too");
              ten_env.return_result(std::move(cmd_result));
            }
          });
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> hello_world_4_cmd;
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world_1") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world 1, too");
      ten_env.return_result(std::move(cmd_result));
    } else if (cmd->get_name() == "hello_world_2") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world 2, too");
      ten_env.return_result(std::move(cmd_result));
    } else if (cmd->get_name() == "hello_world_3") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world 3, too");
      ten_env.return_result(std::move(cmd_result));
    } else if (cmd->get_name() == "hello_world_5") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world 5, too");
      ten_env.return_result(std::move(cmd_result));
    } else if (cmd->get_name() == "hello_world_6") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world 6, too");
      ten_env.return_result(std::move(cmd_result));
    }
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(resp_handler_basic__extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(resp_handler_basic__extension_2,
                                    test_extension_2);

}  // namespace

TEST(ExtensionTest, RespHandlerBasic) {  // NOLINT
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
               "addon": "resp_handler_basic__extension_1",
               "app": "msgpack://127.0.0.1:8001/",
              "extension_group": "resp_handler_basic__extension_group"
             },{
               "type": "extension",
               "name": "test_extension_2",
               "addon": "resp_handler_basic__extension_2",
               "app": "msgpack://127.0.0.1:8001/",
              "extension_group": "resp_handler_basic__extension_group"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "test_extension_1",
               "cmd": [{
                 "name": "hello_world_1",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               },{
                 "name": "hello_world_2",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               },{
                 "name": "hello_world_3",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               },{
                 "name": "hello_world_5",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               },{
                 "name": "hello_world_6",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }]
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_1_cmd = ten::cmd_t::create("hello_world_1");
  hello_world_1_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                              "resp_handler_basic__extension_group",
                              "test_extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_1_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world 1, too");
  auto hello_world_2_cmd = ten::cmd_t::create("hello_world_2");
  hello_world_2_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                              "resp_handler_basic__extension_group",
                              "test_extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_2_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world 2, too");
  auto hello_world_3_cmd = ten::cmd_t::create("hello_world_3");
  hello_world_3_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                              "resp_handler_basic__extension_group",
                              "test_extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_3_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world 3, too");
  auto hello_world_4_cmd = ten::cmd_t::create("hello_world_4");
  hello_world_4_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                              "resp_handler_basic__extension_group",
                              "test_extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_4_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world 4, too");
  auto hello_world_5_cmd = ten::cmd_t::create("hello_world_5");
  hello_world_5_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                              "resp_handler_basic__extension_group",
                              "test_extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_5_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world 5, too");

  delete client;

  ten_thread_join(app_thread, -1);
}