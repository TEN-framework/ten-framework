//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/close_app.h"
#include "ten_utils/lang/cpp/lib/error.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#define TEST_DATA 12344321

namespace {

bool result_handler_is_called = false;

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    // clang-format off

    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(ten_env,
                 R"({
                      "type": "extension",
                      "name": "failed_to_send_cmd__extension_1",
                      "version": "0.1.0",
                      "api": {
                        "cmd_out": [
                          {
                            "name": "test",
                            "property": {
                              "test_data": {
                                "type": "int32"
                              }
                            }
                          }
                        ]
                      }
                    })");
    // clang-format on
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      auto new_cmd = ten::cmd_t::create("test");
      new_cmd->set_property("test_data", TEST_DATA);

      ten::error_t err;
      bool rc = ten_env.send_cmd(
          std::move(new_cmd),
          [](ten::ten_env_t &ten_env,
             std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
            result_handler_is_called = true;

            // TODO(Wei): The `cmd_result` and `cmd` should be empty.
            // Additionally, the value of `err` should equal `TEN_IS_CLOSED`.
          },
          &err);
      EXPECT_EQ(rc, true);

      auto close_app_cmd = ten::cmd_close_app_t::create();
      close_app_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
      ten_env.send_cmd(std::move(close_app_cmd));
    }
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      auto const test_data = cmd->get_property_int32("test_data");
      TEN_ASSERT(test_data == TEST_DATA, "Invalid argument.");
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

void *test_app_thread_main(TEN_UNUSED void *arg) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(failed_to_send_cmd__extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(failed_to_send_cmd__extension_2,
                                    test_extension_2);

}  // namespace

TEST(FailedToTransferMsgTest, FailedToSendCmd) {  // NOLINT
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
               "name": "failed_to_send_cmd__extension_1",
               "addon": "failed_to_send_cmd__extension_1",
               "extension_group": "basic_extension_group_1",
               "app": "msgpack://127.0.0.1:8001/"
             },{
               "type": "extension",
               "name": "failed_to_send_cmd__extension_2",
               "addon": "failed_to_send_cmd__extension_2",
               "extension_group": "basic_extension_group_2",
               "app": "msgpack://127.0.0.1:8001/"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "failed_to_send_cmd__extension_1",
               "cmd": [{
                 "name": "test",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "failed_to_send_cmd__extension_2"
                 }]
               }]
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "basic_extension_group_1",
                            "failed_to_send_cmd__extension_1");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  TEN_ASSERT(!cmd_result, "Should not happen");

  delete client;

  ten_thread_join(app_thread, -1);

  EXPECT_EQ(result_handler_is_called, true);
}
