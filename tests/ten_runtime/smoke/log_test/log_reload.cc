//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

constexpr const char *kReloadedLogPath = "advanced_log_reloaded.log";
constexpr const char *kLogAfterReload = "log emitted after reload";
constexpr const char *kLogAfterInvalidReload =
    "log emitted after invalid reload";

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "reload_log") {
      send_reload_log_cmd(ten_env, std::move(cmd),
                          R"({
            "handlers": [{
              "matchers": [{ "level": "info" }],
              "formatter": {
                "type": "plain",
                "colored": false
              },
              "emitter": {
                "type": "file",
                "config": { "path": "advanced_log_reloaded.log" }
              }
            }]
          })");
      return;
    } else if (cmd->get_name() == "reload_log_invalid") {
      send_reload_log_cmd(ten_env, std::move(cmd), R"({})");
      return;
    } else if (cmd->get_name() == "log_after_reload") {
      TEN_ENV_LOG_INFO(ten_env, kLogAfterReload);
    } else if (cmd->get_name() == "log_after_invalid_reload") {
      TEN_ENV_LOG_INFO(ten_env, kLogAfterInvalidReload);
    } else {
      return;
    }

    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    ten_env.return_result(std::move(cmd_result));
  }

 private:
  static void send_reload_log_cmd(ten::ten_env_t &ten_env,
                                  std::unique_ptr<ten::cmd_t> origin_cmd,
                                  const char *log_config) {
    auto origin_cmd_holder =
        std::make_shared<std::unique_ptr<ten::cmd_t>>(std::move(origin_cmd));

    auto reload_log_cmd = ten::cmd_t::create("ten:reload_log");
    TEN_ASSERT(reload_log_cmd->set_property_from_json(nullptr, log_config),
               "Failed to set reload log configuration.");
    reload_log_cmd->set_dests({{""}});

    bool success = ten_env.send_cmd(
        std::move(reload_log_cmd),
        [origin_cmd_holder](
            ten::ten_env_t &ten_env,
            std::unique_ptr<ten::cmd_result_t> reload_log_result,
            ten::error_t * /* err */) {
          auto result = ten::cmd_result_t::create(
              reload_log_result->get_status_code(), **origin_cmd_holder);
          ten_env.return_result(std::move(result));
        });
    TEN_ASSERT(success, "Failed to send reload log command.");
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
                 "reloadable": true,
                 "handlers": [
                   {
                     "matchers": [{ "level": "info" }],
                     "formatter": {
                       "type": "plain",
                       "colored": false
                     },
                     "emitter": {
                       "type": "console",
                       "config": { "stream": "stdout" }
                     }
                   }
                 ]
               }
             }
           })",
        // clang-format on
        nullptr);
    ASSERT_TRUE(rc);

    ten_env.on_configure_done();
  }
};

void *test_app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

std::string read_file(const char *path) {
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(log_reload__test_extension, test_extension);

}  // namespace

TEST(AdvancedLogTest, LogReload) {  // NOLINT
  std::remove(kReloadedLogPath);

  ten_shared_ptr_t *c_cmd = ten_cmd_create("ten:reload_log", nullptr);
  ASSERT_NE(c_cmd, nullptr);
  EXPECT_EQ(ten_msg_get_type(c_cmd), TEN_MSG_TYPE_CMD_RELOAD_LOG);
  ten_shared_ptr_destroy(c_cmd);

  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  auto start_graph_cmd = ten::start_graph_cmd_t::create();
  start_graph_cmd->set_graph_from_json(R"({
    "nodes": [{
      "type": "extension",
      "name": "test_extension",
      "addon": "log_reload__test_extension",
      "extension_group": "test_extension_group",
      "app": "msgpack://127.0.0.1:8001/"
    }]
  })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  auto reload_log_cmd = ten::cmd_t::create("reload_log");
  reload_log_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result = client->send_cmd_and_recv_result(std::move(reload_log_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  auto log_cmd = ten::cmd_t::create("log_after_reload");
  log_cmd->set_dests({{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result = client->send_cmd_and_recv_result(std::move(log_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  auto invalid_reload_log_cmd = ten::cmd_t::create("reload_log_invalid");
  invalid_reload_log_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result =
      client->send_cmd_and_recv_result(std::move(invalid_reload_log_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_ERROR);

  auto log_after_invalid_cmd = ten::cmd_t::create("log_after_invalid_reload");
  log_after_invalid_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "", "test_extension"}});
  cmd_result =
      client->send_cmd_and_recv_result(std::move(log_after_invalid_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  ten_sleep_ms(1000);
  const std::string log_contents = read_file(kReloadedLogPath);
  EXPECT_NE(log_contents.find(kLogAfterReload), std::string::npos);
  EXPECT_NE(log_contents.find(kLogAfterInvalidReload), std::string::npos);

  delete client;
  ten_thread_join(app_thread, -1);
}
