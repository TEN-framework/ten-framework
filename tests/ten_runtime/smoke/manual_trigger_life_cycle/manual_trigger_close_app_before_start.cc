//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_a : public ten::extension_t {
 public:
  explicit test_extension_a(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    TEN_LOGI("Extension A on_start: %ld",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());

    ten_env.on_start_done();
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    TEN_LOGI("Extension A on_stop: %ld",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());

    // Sleep 1 second
    ten_random_sleep_range_ms(1000, 2000);

    // Send a cmd to extension B to check whether it is stopped
    auto check_stop_cmd = ten::cmd_t::create("check_stop");
    check_stop_cmd->set_dests({{"", "", "test_extension_b"}});

    ten_env.send_cmd(
        std::move(check_stop_cmd),
        [](ten::ten_env_t &ten_env,
           std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
          ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
          bool stopped = cmd_result->get_property_bool("stopped");
          // Extension B should not be stopped yet as it has not been manually
          // triggered by extension A
          ASSERT_EQ(stopped, false);

          // Sleep 1 second then send trigger_life_cycle stop command to
          // extension B
          ten_random_sleep_range_ms(1000, 2000);

          auto trigger_cmd = ten::trigger_life_cycle_cmd_t::create();
          trigger_cmd->set_stage("stop");
          trigger_cmd->set_dests({{"", "", "test_extension_b"}});

          ten_env.send_cmd(std::move(trigger_cmd),
                           [](ten::ten_env_t &ten_env,
                              std::unique_ptr<ten::cmd_result_t> cmd_result,
                              ten::error_t *err) {
                             ten_test::check_status_code(cmd_result,
                                                         TEN_STATUS_CODE_OK);

                             ten_env.on_stop_done();
                           });
        });
  }
};

class test_extension_b : public ten::extension_t {
 public:
  explicit test_extension_b(const char *name) : ten::extension_t(name) {}

  void on_init(ten::ten_env_t &ten_env) override {
    TEN_LOGI("Extension B on_init: %ld",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());

    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    // Create a thread to notify the start event
    thread_ = std::thread([ten_env_proxy]() {
      ten_random_sleep_range_ms(1000, 2000);

      ten_env_proxy->notify([](ten::ten_env_t &ten_env) {
        // Close app to stop the test.
        auto close_app_cmd = ten::close_app_cmd_t::create();
        close_app_cmd->set_dests({{""}});
        ten_env.send_cmd(std::move(close_app_cmd));
      });

      ten_random_sleep_range_ms(1000, 2000);

      ten_env_proxy->notify([](ten::ten_env_t &ten_env,
                               void *user_data) { ten_env.on_init_done(); },
                            nullptr);
      delete ten_env_proxy;
    });
  }

  void on_start(ten::ten_env_t &ten_env) override {
    TEN_LOGI("Extension B on_start (manually triggered): %ld",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());

    started_ = true;

    ten_env.on_start_done();
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    TEN_LOGI("Extension B on_stop (manually triggered): %ld",
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());

    thread_.join();
    stopped_ = true;

    ten_env.on_stop_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "check_start") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("started", started_);
      ten_env.return_result(std::move(cmd_result));
      return;
    }

    if (cmd->get_name() == "check_stop") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("stopped", stopped_);
      ten_env.return_result(std::move(cmd_result));
      return;
    }
  }

 private:
  bool started_{false};
  bool stopped_{false};

  std::thread thread_;
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
           })",
        // clang-format on
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    manual_trigger_close_app_before_start__test_extension_a, test_extension_a);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    manual_trigger_close_app_before_start__test_extension_b, test_extension_b);

}  // namespace

TEST(ManualTriggerLifeCycleTest, CloseAppBeforeStart) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph with extension B configured for manual trigger
  auto start_graph_cmd = ten::start_graph_cmd_t::create();
  start_graph_cmd->set_graph_from_json(R"({
    "nodes": [{
        "type": "extension",
        "name": "test_extension_a",
        "addon": "manual_trigger_close_app_before_start__test_extension_a",
        "extension_group": "a",
        "app": "msgpack://127.0.0.1:8001/"
      },{
        "type": "extension",
        "name": "test_extension_b",
        "addon": "manual_trigger_close_app_before_start__test_extension_b",
        "extension_group": "b",
        "app": "msgpack://127.0.0.1:8001/",
        "property": {
          "ten": {
            "manual_trigger_life_cycle": [
              {
                "stage": "start"
              },
              {
                "stage": "stop"
              }
            ]
          }
        }
      }]
    })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Wait for the app to auto close
  ten_thread_join(app_thread, -1);

  delete client;
}
