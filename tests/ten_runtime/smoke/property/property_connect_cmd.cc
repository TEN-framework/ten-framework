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
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/macros.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#define PROP_NAME "test_prop"
#define APP_PROP_VAL 62422
#define CONNECT_CMD_PROP_VAL 1568

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      auto prop_value = ten_env.get_property_int64(PROP_NAME);
      if (prop_value == CONNECT_CMD_PROP_VAL) {
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property("detail", "hello world, too");
        ten_env.return_result(std::move(cmd_result));
      }
    }
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env,
        // clang-format off
                 "{\
                    \"type\": \"app\",\
                    \"name\": \"test_app\",\
                    \"version\": \"1.0.0\",\
                    \"api\": {\
                      \"property\": {\
                        \"" PROP_NAME "\": {\
                          \"type\": \"int64\"\
                        }\
                      }\
                    }\
                  }"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        "{\
                     \"ten\": {\
                     \"uri\": \"msgpack://127.0.0.1:8001/\"},\
                     \"" PROP_NAME "\": " TEN_XSTR(APP_PROP_VAL) "}");
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(property_start_graph_cmd__extension,
                                    test_extension);

}  // namespace

TEST(PropertyTest, ConnectCmd) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  nlohmann::json start_graph_cmd_content_str =
      R"({
           "nodes": [{
             "type": "extension",
             "name": "test_extension",
             "app": "msgpack://127.0.0.1:8001/",
             "extension_group": "property_start_graph_cmd",
             "addon": "property_start_graph_cmd__extension",
             "property": {}
           }]
         })"_json;
  start_graph_cmd_content_str["nodes"][0]["property"]["test_prop"] =
      CONNECT_CMD_PROP_VAL;

  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(
      start_graph_cmd_content_str.dump().c_str());

  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "property_start_graph_cmd", "test_extension");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world, too");

  delete client;

  ten_thread_join(app_thread, -1);
}
