//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/common/status_code.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_normal_extension : public ten::extension_t {
 public:
  explicit test_normal_extension(const char *name) : ten::extension_t(name) {}
};

class test_predefined_graph : public ten::extension_t {
 public:
  explicit test_predefined_graph(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_graph_cmd = ten::cmd_start_graph_t::create();
    start_graph_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
    start_graph_cmd->set_graph_from_json(R"({
      "nodes": [{
        "type": "extension",
        "name": "normal_extension",
        "addon": "not_existed_extension_addon",
        "app": "msgpack://127.0.0.1:8001/",
        "extension_group": "start_incorrect_graph_1__normal_extension_group"
      }]
    })"_json.dump()
                                             .c_str());

    ten_env.send_cmd(std::move(start_graph_cmd),
                     [this](ten::ten_env_t &ten_env,
                            std::unique_ptr<ten::cmd_result_t> cmd_result,
                            ten::error_t *err) {
                       // result for the 'start_graph' command
                       auto status_code = cmd_result->get_status_code();
                       EXPECT_EQ(status_code, TEN_STATUS_CODE_ERROR);

                       auto graph_id =
                           cmd_result->get_property_string("detail");
                       EXPECT_EQ(graph_id, "");

                       start_graph_cmd_is_done = true;

                       if (test_cmd != nullptr) {
                         nlohmann::json detail = {{"id", 1}, {"name", "a"}};

                         auto cmd_result_for_test = ten::cmd_result_t::create(
                             TEN_STATUS_CODE_OK, *test_cmd);
                         cmd_result_for_test->set_property_from_json(
                             "detail", detail.dump().c_str());
                         ten_env.return_result(std::move(cmd_result_for_test));
                       }
                     });

    ten_env.on_start_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      if (start_graph_cmd_is_done) {
        nlohmann::json detail = {{"id", 1}, {"name", "a"}};

        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property_from_json("detail", detail.dump().c_str());
        ten_env.return_result(std::move(cmd_result));
      } else {
        test_cmd = std::move(cmd);
        return;
      }
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

 private:
  bool start_graph_cmd_is_done{};
  std::unique_ptr<ten::cmd_t> test_cmd;
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

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "log": {
                          "level": 2
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": false,
                          "singleton": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "predefined_graph",
                            "addon": "start_incorrect_graph_1__predefined_graph_extension",
                            "extension_group": "start_incorrect_graph_1__predefined_graph_group"
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

void *app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    start_incorrect_graph_1__predefined_graph_extension, test_predefined_graph);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(start_incorrect_graph_1__normal_extension,
                                    test_normal_extension);

}  // namespace

TEST(StartGraphTest, StartIncorrectGraph1) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Do not need to send 'start_graph' command first.
  // The 'graph_id' MUST be "default" (a special string) if we want to send the
  // request to predefined graph.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                     "start_incorrect_graph_1__predefined_graph_group",
                     "predefined_graph");
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_json(cmd_result, R"({"id": 1, "name": "a"})");

  delete client;

  ten_thread_join(app_thread, -1);
}
