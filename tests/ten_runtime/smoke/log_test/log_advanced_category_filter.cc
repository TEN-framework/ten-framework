//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
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
    // This DEBUG log from extension should appear (category is extension name).
    TEN_ENV_LOG_DEBUG(ten_env, "extension_debug_log_should_appear");

    // This INFO log from extension should appear.
    TEN_ENV_LOG_INFO(ten_env, "extension_info_log_should_appear");

    if (cmd->get_name() == "hello_world") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      ten_env.return_result(std::move(cmd_result));
    }
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    // Log config:
    // - Handler 1: ten:runtime category only logs INFO and above
    // - Handler 2: ten:runtime is OFF, everything else logs DEBUG and above
    // This means ten:runtime DEBUG logs should be filtered out.
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
                         "category": "ten:runtime",
                         "level": "info"
                       }
                     ],
                     "formatter": {
                       "type": "plain",
                       "colored": false
                     },
                     "emitter": {
                       "type": "file",
                       "config": {
                         "path": "category_filter_test.log"
                       }
                     }
                   },
                   {
                     "matchers": [
                       {
                         "category": "ten:runtime",
                         "level": "off"
                       },
                       {
                         "level": "debug"
                       }
                     ],
                     "formatter": {
                       "type": "plain",
                       "colored": false
                     },
                     "emitter": {
                       "type": "file",
                       "config": {
                         "path": "category_filter_test.log"
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(log_advanced_category_filter__test_extension,
                                    test_extension);

}  // namespace

TEST(AdvancedLogTest, LogAdvancedCategoryFilter) {  // NOLINT
  // Remove old log file if exists.
  std::remove("category_filter_test.log");

  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  auto start_graph_cmd = ten::start_graph_cmd_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
                "type": "extension",
                "name": "test_extension",
                "addon": "log_advanced_category_filter__test_extension",
                "extension_group": "test_extension_group",
                "app": "msgpack://127.0.0.1:8001/"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  delete client;

  ten_thread_join(app_thread, -1);

  // Read log file and verify category filtering.
  std::ifstream log_file("category_filter_test.log");
  ASSERT_TRUE(log_file.good()) << "Log file should exist";

  std::stringstream buffer;
  buffer << log_file.rdbuf();
  std::string log_content = buffer.str();

  // Extension logs should appear (they have extension name as category).
  EXPECT_TRUE(log_content.find("extension_debug_log_should_appear") !=
              std::string::npos)
      << "Extension debug log should appear";
  EXPECT_TRUE(log_content.find("extension_info_log_should_appear") !=
              std::string::npos)
      << "Extension info log should appear";

  // ten:runtime DEBUG logs should be filtered out by category filter.
  // Look for typical runtime debug messages that contain "D ten:runtime".
  bool has_runtime_debug = false;
  std::istringstream iss(log_content);
  std::string line;
  while (std::getline(iss, line)) {
    // Check for DEBUG level logs from ten:runtime category.
    if (line.find(" D ") != std::string::npos &&
        line.find("ten:runtime") != std::string::npos) {
      has_runtime_debug = true;
      break;
    }
  }
  EXPECT_FALSE(has_runtime_debug)
      << "ten:runtime DEBUG logs should be filtered out";
}
