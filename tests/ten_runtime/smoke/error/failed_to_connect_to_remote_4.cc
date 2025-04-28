//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/start_graph.h"
#include "ten_runtime/binding/cpp/detail/ten_env.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lib/time.h"
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
        });

    ten_env.on_start_done();

    // Add some random delays to test different timings.
    ten_random_sleep_range_ms(0, 100);

    auto close_app_cmd = ten::cmd_close_app_t::create();
    close_app_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
    ten_env.send_cmd(std::move(close_app_cmd));
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
                        "one_event_loop_per_engine": true,
                        "long_running_mode": true,
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": true,
                          "singleton": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "predefined_graph",
                            "app": "msgpack://127.0.0.1:8001/",
                            "addon": "failed_to_connect_to_remote_4__predefined_graph_extension",
                            "extension_group": "failed_to_connect_to_remote_4__predefined_graph_group"
                          }]
                        },{
                          "name": "graph_1",
                          "auto_start": false,
                          "nodes": [{
                            "type": "extension",
                            "name": "normal_extension_1",
                            "app": "msgpack://127.0.0.1:8001/",
                            "addon": "failed_to_connect_to_remote_4__normal_extension_1",
                            "extension_group": "failed_to_connect_to_remote_4__normal_extension_group"
                          }, {
                            "type": "extension",
                            "name": "normal_extension_2",
                            "app": "msgpack://127.0.0.1:8888/",
                            "addon": "failed_to_connect_to_remote_4__normal_extension_2",
                            "extension_group": "failed_to_connect_to_remote_4__normal_extension_group"
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
    failed_to_connect_to_remote_4__predefined_graph_extension,
    test_predefined_graph);

}  // namespace

TEST(ExtensionTest, FailedToConnectToRemote4) {  // NOLINT
  auto *app_1_thread =
      ten_thread_create("app thread 1", app_thread_1_main, nullptr);

  ten_thread_join(app_1_thread, -1);
}
