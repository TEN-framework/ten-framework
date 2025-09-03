//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <atomic>
#include <csignal>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/ten_env_proxy.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

int g_log_count = 0;

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    // Start a thread to log messages.
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    log_thread_ = std::thread([this, ten_env_proxy]() {
      while (!stop_log_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ten_env_proxy->notify([](ten::ten_env_t &ten_env) {
          for (int i = 0; i < 10; ++i) {
            auto log_msg =
                std::string("log message ") + std::to_string(++g_log_count);
#if !defined(OS_WINDOWS)
            (void)dprintf(STDERR_FILENO, "log_msg: %s\n", log_msg.c_str());
#endif
            TEN_ENV_LOG_INFO(ten_env, log_msg.c_str());
          }
        });
      }

      delete ten_env_proxy;
    });

    ten_env.on_start_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    TEN_ENV_LOG_DEBUG(ten_env,
                      (std::string("on_cmd ") + cmd->get_name()).c_str());

    if (cmd->get_name() == "hello_world") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      ten_env.return_result(std::move(cmd_result));
    }
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    // Stop the thread to log messages.
    stop_log_.store(true);

    if (log_thread_.joinable()) {
      log_thread_.join();
    }

    ten_env.on_stop_done();
  }

 private:
  std::thread log_thread_;
  std::atomic<bool> stop_log_{false};
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
                                  "level": "debug"
                                }
                              ],
                              "formatter": {
                                "type": "json",
                                "colored": false
                              },
                              "emitter": {
                                "type": "file",
                                "config": {
                                  "path": "aaa/log_advanced_file_reopen.log"
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(log_advanced_file_reopen__test_extension,
                                    test_extension);

}  // namespace

TEST(AdvancedLogTest, LogAdvancedFileReopen) {  // NOLINT
  // Remove the log file if it already exists.
  std::string log_file_path = "aaa/log_advanced_file_reopen.log";
  std::ifstream check_file(log_file_path);
  if (check_file.good()) {
    check_file.close();
    ASSERT_EQ(std::remove(log_file_path.c_str()), 0)
        << "Failed to remove existing log file";
  }

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
                "addon": "log_advanced_file_reopen__test_extension",
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

  // On Unix-like systems, we can use the SIGHUP signal to reload the log file.
#ifndef _WIN32
  // Wait for 3 seconds.
  std::this_thread::sleep_for(std::chrono::seconds(3));

  {
    // Send a signal to reload the log file.
    auto rc = raise(SIGHUP);
    ASSERT_EQ(rc, 0);
  }

  // Wait for another 3 seconds.
  std::this_thread::sleep_for(std::chrono::seconds(3));

  {
    // Send a signal to reload the log file.
    auto rc = raise(SIGHUP);
    ASSERT_EQ(rc, 0);
  }
#endif

  delete client;

  ten_thread_join(app_thread, -1);

  // Sleep 3 seconds to wait for the log file to be flushed. For example, in mac
  // (release) build, if not sleep 3 seconds, the operating system may not have
  // written the log, the test case may start to check the log.
  ten_sleep_ms(3000);

#ifndef _WIN32
  // Check the log file content.
  std::ifstream log_file("aaa/log_advanced_file_reopen.log");
  EXPECT_TRUE(log_file.good());

  // Make sure the log content contains "log message 1" to "log message
  // {g_log_count}". Make sure that no logs are lost.
  std::string line;
  std::vector<bool> found(g_log_count, false);
  int total_found = 0;

  while (std::getline(log_file, line)) {
    size_t pos = line.find("log message ");
    if (pos != std::string::npos) {
      int msg_num = 0;
      try {
        msg_num = std::stoi(line.substr(pos + 12));
        if (msg_num > 0 && msg_num <= g_log_count) {
          if (!found[msg_num - 1]) {
            found[msg_num - 1] = true;
            total_found++;
          }
        }
      } catch (const std::exception &e) {
        continue;
      }
    }
  }

  if (total_found != g_log_count) {
    std::cout << "Expected " << g_log_count << " messages, but found "
              << total_found << '\n';

    std::cout << "\nlog file content:\n";
    log_file.clear();
    log_file.seekg(0);
    std::string content;
    while (std::getline(log_file, content)) {
      std::cout << content << '\n';
    }

    std::cout << "\nmissing message numbers:\n";
    for (int i = 0; i < g_log_count; ++i) {
      if (!found[i]) {
        std::cout << "log message " << (i + 1) << '\n';
      }
    }

    FAIL();
  }
#endif
}
