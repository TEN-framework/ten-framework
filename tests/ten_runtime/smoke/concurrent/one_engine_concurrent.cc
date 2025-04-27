//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "nlohmann/json_fwd.hpp"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/common/constant.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_A : public ten::extension_t {
 public:
  explicit test_extension_A(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    ten_env.send_cmd(std::move(cmd));
  }
};

class test_extension_B : public ten::extension_t {
 public:
  explicit test_extension_B(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto detail = nlohmann::json({{"a", "b"}});
    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property_from_json("detail", detail.dump().c_str());
    ten_env.return_result(std::move(cmd_result));
  }
};

class test_app_a : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "long_running_mode": true,
                        "log": {
                          "level": 2
                        }
                      }
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

class test_app_b : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8002/",
                        "long_running_mode": true,
                        "log": {
                          "level": 2
                        }
                      }
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *app_thread_1_main(TEN_UNUSED void *args) {
  auto *app_a = new test_app_a();
  app_a->run();
  delete app_a;

  return nullptr;
}

void *app_thread_2_main(TEN_UNUSED void *args) {
  auto *app_b = new test_app_b();
  app_b->run();
  delete app_b;

  return nullptr;
}

std::string graph_id;

void *client_thread_main(TEN_UNUSED void *args) {
  auto seq_id = (size_t)args;

  TEN_LOGD("Client[%zu]: start.", seq_id);

  auto seq_id_str = std::to_string(seq_id);

  // Connect again, send request with graph_id directly.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  std::string client_ip;
  uint16_t client_port = 0;
  client->get_info(client_ip, client_port);
  TEN_LOGD("Client[%zu] ip address: %s:%d", seq_id, client_ip.c_str(),
           client_port);

  // Send a user-defined 'hello world' command.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8001/", graph_id.c_str(),
                     "extension_group_A", "A");

  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));

  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_json(cmd_result, R"({"a": "b"})");

  // Destroy the client.
  delete client;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(one_engine_concurrent__extension_A,
                                    test_extension_A);

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(one_engine_concurrent__extension_B,
                                    test_extension_B);

}  // namespace

TEST(ExtensionTest, OneEngineConcurrent) {  // NOLINT
  // Start app.
  auto *app_thread_2 =
      ten_thread_create("app thread 2", app_thread_2_main, nullptr);
  auto *app_thread_1 =
      ten_thread_create("app thread 1", app_thread_1_main, nullptr);

  // Create a client and connect to the app.
  ten::msgpack_tcp_client_t *client = nullptr;

  for (size_t i = 0; i < MULTIPLE_APP_SCENARIO_GRAPH_CONSTRUCTION_RETRY_TIMES;
       ++i) {
    client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

    // Send graph.
    auto start_graph_cmd = ten::cmd_start_graph_t::create();
    start_graph_cmd->set_long_running_mode(true);
    start_graph_cmd->set_graph_from_json(
        R"({
             "nodes": [{
               "type": "extension",
               "name": "A",
               "addon": "one_engine_concurrent__extension_A",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "extension_group_A"
             },{
               "type": "extension",
               "name": "B",
               "addon": "one_engine_concurrent__extension_B",
               "app": "msgpack://127.0.0.1:8002/",
               "extension_group": "extension_group_B"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "A",
               "cmd": [{
                 "name": "test",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8002/",
                   "extension": "B"
                 }]
               }]
             }]
           })");

    auto cmd_result =
        client->send_cmd_and_recv_result(std::move(start_graph_cmd));

    if (cmd_result) {
      ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
      graph_id = cmd_result->get_property_string("detail");

      break;
    } else {
      delete client;
      client = nullptr;

      // To prevent from busy re-trying.
      ten_random_sleep_range_ms(10, 20);
    }
  }

  TEN_ASSERT(client, "Failed to connect to the TEN app.");

  // Now close connection. The engine will be alive due to 'long_running_mode'.
  delete client;

  std::vector<ten_thread_t *> client_threads;

  for (size_t i = 0; i < ONE_ENGINE_ALL_CLIENT_CONCURRENT_CNT; ++i) {
    auto *client_thread =
        ten_thread_create("client_thread_main", client_thread_main, (void *)i);

    client_threads.push_back(client_thread);
  }

  for (ten_thread_t *client_thread : client_threads) {
    ten_thread_join(client_thread, -1);
  }

  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8001/");
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8002/");

  ten_thread_join(app_thread_1, -1);
  ten_thread_join(app_thread_2, -1);
}
