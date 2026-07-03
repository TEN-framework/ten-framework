//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/stop_graph_cmd.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_graph_cmd = ten::start_graph_cmd_t::create();
    start_graph_cmd->set_dests({{""}});
    start_graph_cmd->set_graph_from_json(R"({
      "nodes": [{
        "type": "extension",
        "name": "test_extension_2",
        "addon": "graph_communication_basic__test_extension_2",
        "app": "msgpack://127.0.0.1:8001/",
        "extension_group": "graph_communication_basic__test_extension_2_group"
      }],
      "exposed_messages": [
        {
          "type": "cmd_in",
          "name": "hello_world",
          "extension": "test_extension_2"
        },
        {
          "type": "cmd_out",
          "name": "good_bye",
          "extension": "test_extension_2"
        }
      ]
    })"_json.dump()
                                             .c_str());

    ten_env.send_cmd(
        std::move(start_graph_cmd),
        [](ten::ten_env_t &ten_env,
           std::unique_ptr<ten::cmd_result_t> cmd_result,
           ten::error_t * /* err */) {
          // The graph_id is in the response of the 'start_graph' command.
          auto graph_id = cmd_result->get_property_string("graph_id");

          auto hello_world_cmd = ten::cmd_t::create("hello_world");
          // Set dests to the graph, the msg will automatically be sent to the
          // 'test_extension_2' extension in the graph.
          hello_world_cmd->set_dests(
              {{"msgpack://127.0.0.1:8001/", graph_id.c_str()}});
          ten_env.send_cmd(
              std::move(hello_world_cmd),
              [](ten::ten_env_t &ten_env,
                 std::unique_ptr<ten::cmd_result_t> cmd_result,
                 ten::error_t * /* err */) {
                TEN_ASSERT(cmd_result->get_status_code() == TEN_STATUS_CODE_OK,
                           "Should not happen.");
              });
        });

    ten_env.on_start_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      if (start_and_stop_graph_is_completed) {
        // Send the response to the client.
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);

        nlohmann::json detail = {{"id", 1}, {"name", "a"}};
        cmd_result->set_property_from_json("detail", detail.dump().c_str());

        ten_env.return_result(std::move(cmd_result));
      } else {
        // Save the command for later use. This is the command from the client.
        test_cmd = std::move(cmd);
      }
    } else if (cmd->get_name() == "good_bye") {
      auto graph_id = cmd->get_source().graph_id;

      ten_env.return_result(
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd));

      // Shut down the graph; otherwise, the app won't be able to
      // close because there is still a running engine/graph.
      auto stop_graph_cmd = ten::stop_graph_cmd_t::create();

      // stop_graph command should be sent to the app.
      stop_graph_cmd->set_dests({{""}});

      // Tell app to stop the specified graph.
      stop_graph_cmd->set_graph_id(graph_id->c_str());

      ten_env.send_cmd(
          std::move(stop_graph_cmd),
          [this](ten::ten_env_t &ten_env,
                 std::unique_ptr<ten::cmd_result_t> /* cmd_result */,
                 ten::error_t * /* err */) {
            // Received the response from test_extension_2.
            start_and_stop_graph_is_completed = true;

            if (test_cmd != nullptr) {
              // Send the response to the client.
              auto cmd_result_for_test =
                  ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *test_cmd);

              nlohmann::json detail = {{"id", 1}, {"name", "a"}};
              cmd_result_for_test->set_property_from_json(
                  "detail", detail.dump().c_str());

              ten_env.return_result(std::move(cmd_result_for_test));
            }
          });
    } else {
      // Should not receive any other command.
      TEN_ASSERT(0, "Should not happen.");
    }
  }

 private:
  bool start_and_stop_graph_is_completed{};
  std::unique_ptr<ten::cmd_t> test_cmd;
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      // Send the response to test_extension_1.
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      ten_env.return_result(std::move(cmd_result));

      // Send another command to test_extension_1.
      auto good_bye_cmd = ten::cmd_t::create("good_bye");
      ten_env.send_cmd(std::move(good_bye_cmd));
    }
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env,
        // clang-format off
        R"({
             "type": "app",
             "name": "test_app",
             "version": "0.1.0"
           })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    // Set the predefined graph.
    rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8001/",
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
               },
               "predefined_graphs": [{
                 "name": "default",
                 "auto_start": false,
                 "singleton": true,
                 "graph": {
                   "nodes": [{
                     "type": "extension",
                     "name": "test_extension_1",
                     "addon": "graph_communication_basic__test_extension_1",
                     "extension_group": "graph_communication_basic__predefined_graph_group"
                   }]
                 }
               }]
             }
           })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(graph_communication_basic__test_extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(graph_communication_basic__test_extension_2,
                                    test_extension_2);

}  // namespace

TEST(GraphCommunicationTest, GraphCommunicationBasic) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Do not need to send 'start_graph' command first.
  // The 'graph_id' MUST be "default" (a special string) if we want to send the
  // request to predefined graph.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "test_extension_1"}});
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_json(cmd_result, R"({"id": 1, "name": "a"})");

  // Delete the client will trigger the app to exit.
  delete client;

  // Wait for the app to exit.
  ten_thread_join(app_thread, -1);
}
