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
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/common/constant.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#define DEST_EXTENSION_MIN_ID 2
#define DEST_EXTENSION_MAX_ID 15

#define DEFINE_EXTENSION(N)                                                    \
  class test_extension_##N : public ten::extension_t {                         \
   public:                                                                     \
    explicit test_extension_##N(const char *name) : ten::extension_t(name) {}  \
                                                                               \
    void on_cmd(ten::ten_env_t &ten_env,                                       \
                std::unique_ptr<ten::cmd_t> cmd) override {                    \
      nlohmann::json json =                                                    \
          nlohmann::json::parse(cmd->get_property_to_json());                  \
      if (cmd->get_name() == "hello_world") {                                  \
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd); \
        cmd_result->set_property("detail", "hello world from extension " #N);  \
        ten_env.return_result(std::move(cmd_result));                          \
      }                                                                        \
    }                                                                          \
  };

#define DEFINE_APP(N, port)                                                     \
  class test_app_##N : public ten::app_t {                                      \
   public:                                                                      \
    void on_configure(ten::ten_env_t &ten_env) {                                \
      /* clang-format off */                                                  \
      ten_env.init_property_from_json(                                     \
                   "{\"ten\": {\"uri\": \"msgpack://127.0.0.1:" #port "/\",   \
                   \"long_running_mode\": true }                              \
                   }"); \
      /* clang-format on */                                                     \
      ten_env.on_configure_done();                                              \
    }                                                                           \
  };                                                                            \
  static void *test_app_##N##_thread_main(TEN_UNUSED void *args) {              \
    auto *app = new test_app_##N();                                             \
    app->run();                                                                 \
    delete app;                                                                 \
                                                                                \
    return nullptr;                                                             \
  }

#define REGISTER_EXTENSION(N)                                     \
  TEN_CPP_REGISTER_ADDON_AS_EXTENSION(                            \
      multi_dest_in_multi_app_with_result_handler__extension_##N, \
      test_extension_##N);

#define START_APP(N)           \
  auto test_app_##N##_thread = \
      ten_thread_create(nullptr, test_app_##N##_thread_main, nullptr);

#define WAIT_APP_TO_STOP(N) ten_thread_join(test_app_##N##_thread, -1);

namespace {

typedef enum RESPONSE {
  RESPONSE_2,
  RESPONSE_3,
  RESPONSE_4,
  RESPONSE_5,
  RESPONSE_6,
  RESPONSE_7,
  RESPONSE_8,
  RESPONSE_9,
  RESPONSE_10,
  RESPONSE_11,
  RESPONSE_12,
  RESPONSE_13,
  RESPONSE_14,
  RESPONSE_15,
} RESPONSE;

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      ten_env.send_cmd(
          std::move(cmd), [&](ten::ten_env_t &ten_env,
                              std::unique_ptr<ten::cmd_result_t> cmd_result,
                              ten::error_t *err) {
            pending_resp_num--;
            if (pending_resp_num == 0) {
              cmd_result->set_property("detail", "return from extension 1");
              ten_env.return_result(std::move(cmd_result));
            }
          });
    }
  }

 private:
  int pending_resp_num{1};
};

DEFINE_EXTENSION(2)
DEFINE_EXTENSION(3)
DEFINE_EXTENSION(4)
DEFINE_EXTENSION(5)
DEFINE_EXTENSION(6)
DEFINE_EXTENSION(7)
DEFINE_EXTENSION(8)
DEFINE_EXTENSION(9)
DEFINE_EXTENSION(10)
DEFINE_EXTENSION(11)
DEFINE_EXTENSION(12)
DEFINE_EXTENSION(13)
DEFINE_EXTENSION(14)
DEFINE_EXTENSION(15)

REGISTER_EXTENSION(1)
REGISTER_EXTENSION(2)
REGISTER_EXTENSION(3)
REGISTER_EXTENSION(4)
REGISTER_EXTENSION(5)
REGISTER_EXTENSION(6)
REGISTER_EXTENSION(7)
REGISTER_EXTENSION(8)
REGISTER_EXTENSION(9)
REGISTER_EXTENSION(10)
REGISTER_EXTENSION(11)
REGISTER_EXTENSION(12)
REGISTER_EXTENSION(13)
REGISTER_EXTENSION(14)
REGISTER_EXTENSION(15)

}  // namespace

DEFINE_APP(1, 8001)
DEFINE_APP(2, 8002)
DEFINE_APP(3, 8003)
DEFINE_APP(4, 8004)
DEFINE_APP(5, 8005)

TEST(MultiDestTest, MultiDestInMultiAppWithResponseHandler) {  // NOLINT
  // Start app.
  START_APP(1)
  START_APP(2)
  START_APP(3)
  START_APP(4)
  START_APP(5)

  // Create a client and connect to the app.
  ten::msgpack_tcp_client_t *client = nullptr;

  for (size_t i = 0; i < MULTIPLE_APP_SCENARIO_GRAPH_CONSTRUCTION_RETRY_TIMES;
       ++i) {
    client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

    // Send graph.
    auto start_graph_cmd = ten::cmd_start_graph_t::create();
    start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
                 "type": "extension",
                 "name": "test_extension_1",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_1",
                 "app": "msgpack://127.0.0.1:8001/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_1"
               },{
                 "type": "extension",
                 "name": "test_extension_2",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_2",
                 "app": "msgpack://127.0.0.1:8001/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_1"
               },{
                 "type": "extension",
                 "name": "test_extension_3",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_3",
                 "app": "msgpack://127.0.0.1:8001/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_1"
               },{
                 "type": "extension",
                 "name": "test_extension_4",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_4",
                 "app": "msgpack://127.0.0.1:8002/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_2"
               },{
                 "type": "extension",
                 "name": "test_extension_5",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_5",
                 "app": "msgpack://127.0.0.1:8002/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_2"
               },{
                 "type": "extension",
                 "name": "test_extension_6",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_6",
                 "app": "msgpack://127.0.0.1:8002/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_2"
               },{
                 "type": "extension",
                 "name": "test_extension_7",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_7",
                 "app": "msgpack://127.0.0.1:8003/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_3"
               },{
                 "type": "extension",
                 "name": "test_extension_8",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_8",
                 "app": "msgpack://127.0.0.1:8003/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_3"
               },{
                 "type": "extension",
                 "name": "test_extension_9",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_9",
                 "app": "msgpack://127.0.0.1:8003/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_3"
               },{
                 "type": "extension",
                 "name": "test_extension_10",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_10",
                 "app": "msgpack://127.0.0.1:8004/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_4"
               },{
                 "type": "extension",
                 "name": "test_extension_11",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_11",
                 "app": "msgpack://127.0.0.1:8004/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_4"
               },{
                 "type": "extension",
                 "name": "test_extension_12",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_12",
                 "app": "msgpack://127.0.0.1:8004/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_4"
               },{
                 "type": "extension",
                 "name": "test_extension_13",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_13",
                 "app": "msgpack://127.0.0.1:8005/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_5"
               },{
                 "type": "extension",
                 "name": "test_extension_14",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_14",
                 "app": "msgpack://127.0.0.1:8005/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_5"
               },{
                 "type": "extension",
                 "name": "test_extension_15",
                 "addon": "multi_dest_in_multi_app_with_result_handler__extension_15",
                 "app": "msgpack://127.0.0.1:8005/",
                 "extension_group": "multi_dest_in_multi_app_with_result_handler__extension_group_5"
               }],
               "connections": [{
                 "app": "msgpack://127.0.0.1:8001/",
                 "extension": "test_extension_1",
                 "cmd": [{
                   "name": "hello_world",
                   "dest": [{
                      "app": "msgpack://127.0.0.1:8001/",
                      "extension": "test_extension_2"
                   },{
                      "app": "msgpack://127.0.0.1:8001/",
                      "extension": "test_extension_3"
                   },{
                      "app": "msgpack://127.0.0.1:8002/",
                      "extension": "test_extension_4"
                   },{
                      "app": "msgpack://127.0.0.1:8002/",
                      "extension": "test_extension_5"
                   },{
                      "app": "msgpack://127.0.0.1:8002/",
                      "extension": "test_extension_6"
                   },{
                      "app": "msgpack://127.0.0.1:8003/",
                      "extension": "test_extension_7"
                   },{
                      "app": "msgpack://127.0.0.1:8003/",
                      "extension": "test_extension_8"
                   },{
                      "app": "msgpack://127.0.0.1:8003/",
                      "extension": "test_extension_9"
                   },{
                      "app": "msgpack://127.0.0.1:8004/",
                      "extension": "test_extension_10"
                   },{
                      "app": "msgpack://127.0.0.1:8004/",
                      "extension": "test_extension_11"
                   },{
                      "app": "msgpack://127.0.0.1:8004/",
                      "extension": "test_extension_12"
                   },{
                      "app": "msgpack://127.0.0.1:8005/",
                      "extension": "test_extension_13"
                   },{
                      "app": "msgpack://127.0.0.1:8005/",
                      "extension": "test_extension_14"
                   },{
                      "app": "msgpack://127.0.0.1:8005/",
                      "extension": "test_extension_15"
                   }]
                 }]
               }]
             })");
    auto cmd_result =
        client->send_cmd_and_recv_result(std::move(start_graph_cmd));

    if (cmd_result) {
      ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
      break;
    } else {
      delete client;
      client = nullptr;

      // To prevent from busy re-trying.
      ten_random_sleep_range_ms(10, 20);
    }
  }

  TEN_ASSERT(client, "Failed to connect to the TEN app.");

  // Send a user-defined 'hello world' command to 'extension 1'.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest(
      "msgpack://127.0.0.1:8001/", nullptr,
      "multi_dest_in_multi_app_with_result_handler__extension_group_1",
      "test_extension_1");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "return from extension 1");

  delete client;

  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8001/");
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8002/");
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8003/");
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8004/");
  ten::msgpack_tcp_client_t::close_app("msgpack://127.0.0.1:8005/");

  WAIT_APP_TO_STOP(1)
  WAIT_APP_TO_STOP(2)
  WAIT_APP_TO_STOP(3)
  WAIT_APP_TO_STOP(4)
  WAIT_APP_TO_STOP(5)
}