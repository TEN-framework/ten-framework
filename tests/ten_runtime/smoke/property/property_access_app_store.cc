//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_property_access_app_store_1 : public ten::extension_t {
 public:
  explicit test_property_access_app_store_1(const char *name)
      : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property("detail", "success");
    ten_env.return_result(std::move(cmd_result));
  }
};

class test_property_access_app_store_2 : public ten::extension_t {
 public:
  explicit test_property_access_app_store_2(const char *name)
      : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto result = ten_env.get_property_int32("app:aaa");
    if (result == 3) {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "success");
      ten_env.return_result(std::move(cmd_result));
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

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"###({
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
                            "name": "test_property_access_app_store_1",
                            "addon": "test_property_access_app_store_1",
                            "extension_group": "default_extension_group_1"
                          },{
                            "type": "extension",
                            "name": "test_property_access_app_store_2",
                            "addon": "test_property_access_app_store_2",
                            "extension_group": "default_extension_group_2"
                          }]
                        }]
                      },
                      "aaa": 3
                    })###"
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(test_property_access_app_store_1,
                                    test_property_access_app_store_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(test_property_access_app_store_2,
                                    test_property_access_app_store_2);

}  // namespace

TEST(PropertyTest, AccessAppStore) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send a request to test_property_access_app_store_1 to make sure
  // it has been initted.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                     "default_extension_group_1",
                     "test_property_access_app_store_1");
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "success");

  // Do not need to send 'start_graph' command first.
  // The 'graph_id' MUST be "default" (a special string) if we want to send the
  // request to predefined graph.
  test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                     "default_extension_group_2",
                     "test_property_access_app_store_2");
  cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "success");

  delete client;

  ten_thread_join(app_thread, -1);
}
