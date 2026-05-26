//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      hello_world_cmd = std::move(cmd);

      // Send a data message to a non-existent graph. This should NOT crash
      // the app (but currently triggers an assert in
      // ten_app_handle_msg_default_handler).
      auto data = ten::data_t::create("test_data");
      data->set_dests({{"msgpack://127.0.0.1:8001/", "incorrect_graph_id",
                        "test_extension"}});
      ten_env.send_data(std::move(data));

      // Send a cmd to close the app after a short delay to allow the data
      // message to be processed.
      auto check_cmd = ten::cmd_t::create("check");
      check_cmd->set_dests(
          {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
      ten_env.send_cmd(
          std::move(check_cmd),
          [](ten::ten_env_t &ten_env,
             std::unique_ptr<ten::cmd_result_t> cmd_result,
             ten::error_t *err) { return true; });
    } else if (cmd->get_name() == "check") {
      // If we reach here, the app did not crash from the data message.
      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *hello_world_cmd);
      cmd_result->set_property("detail", "data sent without crash");
      ten_env.return_result(std::move(cmd_result));
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> hello_world_cmd;
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "one_event_loop_per_engine": true,
                        "log": {
                          "handlers": [
                            {
                              "matchers": [
                                {
                                  "level": "debug"
                                }
                              ],
                              "formatter": {
                                "type": "plain",
                                "colored": true
                              },
                              "emitter": {
                                "type": "console",
                                "config": {
                                  "stream": "stdout"
                                }
                              }
                            }
                          ]
                        }
                      }
                    })"
        // clang-format on
    );
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
    extension_send_data_to_incorrect_engine__extension, test_extension);

}  // namespace

TEST(ExtensionTest, ExtensionSendDataToIncorrectEngine) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::start_graph_cmd_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
               "type": "extension",
               "name": "test_extension",
               "addon": "extension_send_data_to_incorrect_engine__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_send_data_to_incorrect_engine"
              }]
            })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command which triggers the extension
  // to send a data message to a non-existent graph.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});

  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));

  // After the fix, this should succeed without crashing.
  // Before the fix, the app will crash (abort) due to the assert in
  // ten_app_handle_msg_default_handler when it tries to create a cmd_result
  // from a non-cmd message.
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Destroy the client.
  delete client;

  ten_thread_join(app_thread, -1);
}
