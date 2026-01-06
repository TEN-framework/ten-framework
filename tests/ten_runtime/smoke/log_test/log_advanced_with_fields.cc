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
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_init(ten::ten_env_t &ten_env) override {
    // Test 1: Log with multiple fields of various types using builder pattern
    TEN_ENV_LOG_INFO_WITH_FIELDS(
        ten_env, "Testing structured logging with various field types")
        .field("string_field", "hello world")
        .field("int_field", 42)
        .field("float_field", 3.14159)
        .field("bool_field", true)
        .field("negative_int", -100)
        .field("large_number", 9223372036854775807L);

    // Test 2: Log with nested object (using traditional API for complex
    // structures)
    ten_json_t *json = ten_json_from_string(
        R"({
          "nested_object": {
            "inner_key": "inner_value"
          },
          "array_field": [1, 2, 3, "four", true]
        })",
        nullptr);
    ten::value_t complex_fields;
    complex_fields.from_json(json);
    ten_json_destroy(json);
    TEN_ENV_LOG(ten_env, TEN_LOG_LEVEL_INFO,
                "Testing log with nested object and array", nullptr,
                &complex_fields);

    // Test 3: Log with category
    TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Testing log with category")
        .category("initialization")
        .field("status", "success")
        .field("duration_ms", 150);

    // Test 4: Simple log with single field
    TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Simple log with single field")
        .field("value", 123);

    // Test 5: Log with string types
    TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Testing different string types")
        .field("const_char", "C string")
        .field("std_string", std::string("C++ string"))
        .field("literal", "string literal");

    ten_env.on_init_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Test 6: Log with dynamic values from command
    TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Received command")
        .field("cmd_name", cmd->get_name().c_str())
        .field("timestamp", ten_current_time_ms());

    if (cmd->get_name() == "hello_world") {
      // Test 7: Log command processing with fields
      TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Processing hello_world command")
          .field("cmd_name", "hello_world")
          .field("status", "ok")
          .field("response", "hello world, too");

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      ten_env.return_result(std::move(cmd_result));
    }
  }

  void on_deinit(ten::ten_env_t &ten_env) override {
    // Test 8: Log cleanup with performance metrics
    TEN_ENV_LOG_INFO_WITH_FIELDS(ten_env, "Extension cleanup")
        .category("lifecycle")
        .field("phase", "deinit")
        .field("cleanup_status", "success");

    ten_env.on_deinit_done();
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
                 "handlers": [
                   {
                     "matchers": [
                       {
                         "level": "info"
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
        ,
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(log_advanced_with_fields__test_extension,
                                    test_extension);

}  // namespace

TEST(AdvancedLogTest, LogAdvancedWithFields) {  // NOLINT
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
                "addon": "log_advanced_with_fields__test_extension",
                "extension_group": "test_extension_group",
                "app": "msgpack://127.0.0.1:8001/"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "hello world, too");

  delete client;

  ten_thread_join(app_thread, -1);
}
