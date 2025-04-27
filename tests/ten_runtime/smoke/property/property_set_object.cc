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
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(ten_env,
                                                                        R"({
                      "type": "extension",
                      "name": "schema_cmd_result_error__test_extension_2",
                      "version": "0.1.0",
                      "api": {
                        "cmd_in": [
                          {
                            "name": "hello_world",
                            "property": {
                              "tool": {
                                "type": "object",
                                "properties": {
                                  "name": {
                                    "type": "string"
                                  },
                                  "description": {
                                    "type": "string"
                                  },
                                  "parameters": {
                                    "type": "array",
                                    "items": {
                                      "type": "object",
                                      "properties": {}
                                    }
                                  }
                                }
                              }
                            }
                          }
                        ]
                      }
                    })");
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      // The get/set property actions of the message itself will not immediately
      // trigger schema validation. This is because the message might be
      // manipulated in other threads, and schema information is tied to the
      // extension. For thread safety, schema validation for the message is
      // delayed until it interacts with the extension system, specifically
      // during `send_xxx` and `return_xxx` operations.
      bool rc = cmd->set_property_from_json("tool", R"({
        "name": "hammer",
        "description": "a tool to hit nails",
        "parameters": [ "foo" ]
      })");
      TEN_ASSERT(rc, "Should not happen.");

      rc = cmd->set_property_from_json("tool", R"({
        "name": "hammer",
        "description": "a tool to hit nails",
        "parameters": []
      })");
      TEN_ASSERT(rc, "Should not happen.");

      auto prop_value = cmd->get_property_string("tool.name");
      if (prop_value == "hammer") {
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
                    \"version\": \"1.0.0\"\
                  }"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        "{\
                     \"ten\": {  \
                     \"uri\": \"msgpack://127.0.0.1:8001/\"}}");
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(property_set_object__extension,
                                    test_extension);

}  // namespace

TEST(PropertyTest, SetObject) {  // NOLINT
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
               "name": "test_extension",
               "addon": "property_set_object__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "property_set_object__extension_group"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "property_set_object__extension_group",
                            "test_extension");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world, too");

  delete client;

  ten_thread_join(app_thread, -1);
}
