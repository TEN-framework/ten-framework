//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/start_graph.h"
#include "ten_runtime/common/status_code.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_predefined_graph : public ten::extension_t {
 public:
  explicit test_predefined_graph(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_graph_cmd = ten::cmd_start_graph_t::create();
    start_graph_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
    start_graph_cmd->set_predefined_graph_name("graph_1");
    ten_env.send_cmd(
        std::move(start_graph_cmd),
        [](ten::ten_env_t &ten_env,
           std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
          auto status_code = cmd_result->get_status_code();
          ASSERT_EQ(status_code, TEN_STATUS_CODE_ERROR);

          auto detail = cmd_result->get_property_string("detail");
          ASSERT_EQ(detail, "Failed to connect to msgpack://127.0.0.1:8888/");

          // The app will not be closed because it is running in
          // long_running_mode.

          ten_env.on_start_done();
        });
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      nlohmann::json detail = {{"id", 1}, {"name", "a"}};

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property_from_json("detail", detail.dump().c_str());
      ten_env.return_result(std::move(cmd_result));
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }
};

class test_app_1 : public ten::app_t {
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

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "log": {
                          "level": 2
                        },
                        "long_running_mode": true,
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": false,
                          "singleton": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "predefined_graph",
                            "app": "msgpack://127.0.0.1:8001/",
                            "addon": "failed_to_connect_to_remote_1__predefined_graph_extension",
                            "extension_group": "failed_to_connect_to_remote_1__predefined_graph_group"
                          }]
                        },{
                          "name": "graph_1",
                          "auto_start": false,
                          "nodes": [{
                            "type": "extension",
                            "name": "normal_extension_1",
                            "app": "msgpack://127.0.0.1:8001/",
                            "addon": "failed_to_connect_to_remote_1__normal_extension_1",
                            "extension_group": "failed_to_connect_to_remote_1__normal_extension_group"
                          }, {
                            "type": "extension",
                            "name": "normal_extension_2",
                            "app": "msgpack://127.0.0.1:8888/",
                            "addon": "failed_to_connect_to_remote_1__normal_extension_2",
                            "extension_group": "failed_to_connect_to_remote_1__normal_extension_group"
                          }],
                          "connections": [{
                            "app": "msgpack://127.0.0.1:8001/",
                            "extension": "normal_extension_1",
                            "cmd": [{
                              "name": "hello_world",
                              "dest": [{
                                "app": "msgpack://127.0.0.1:8888/",
                                "extension": "normal_extension_2"
                              }]
                            }]
                          }]
                        }]
                      }
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *app_thread_1_main(TEN_UNUSED void *args) {
  auto *app = new test_app_1();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    failed_to_connect_to_remote_1__predefined_graph_extension,
    test_predefined_graph);

}  // namespace

TEST(ExtensionTest, FailedToConnectToRemote1) {  // NOLINT
  auto *app_1_thread =
      ten_thread_create("app thread 1", app_thread_1_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Do not need to send 'start_graph' command first.
  // The 'graph_id' MUST be "default" if we want to send the request to
  // predefined graph.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                     "failed_to_connect_to_remote_1__predefined_graph_group",
                     "predefined_graph");
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_json(cmd_result, R"({"id": 1, "name": "a"})");

  delete client;

  // Send a close_app command to close the app as the app is running in
  // long_running_mode.
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8001/");

  ten_thread_join(app_1_thread, -1);
}
