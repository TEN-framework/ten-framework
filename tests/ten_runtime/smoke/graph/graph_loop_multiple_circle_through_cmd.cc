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
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#define LOOP_CNT 2

namespace {

/**
 *
 * client --> A --> B --> C
 *                  ^     |
 *                  |     V
 *                  <---- D
 *
 */
class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name)
      : ten::extension_t(name), name_(name) {}

  void on_init(ten::ten_env_t &ten_env) override {
    value_ = ten_env.get_property_int32("value");
    ten_env.on_init_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "sum") {
      if (counter_ == LOOP_CNT) {
        auto json = nlohmann::json::parse(cmd->get_property_to_json());
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property_from_json("detail", json.dump().c_str());
        ten_env.return_result(std::move(cmd_result));
      } else {
        counter_++;

        if (!cmd->is_property_exist("total")) {
          cmd->set_property("total", 0);
        }

        int total = cmd->get_property_int32("total");
        total += value_;

        cmd->set_property("total", total);

        ten_env.send_cmd(std::move(cmd),
                         [](ten::ten_env_t &ten_env,
                            std::unique_ptr<ten::cmd_result_t> cmd_result,
                            ten::error_t *err) {
                           ten_env.return_result(std::move(cmd_result));
                         });
      }
    }
  }

 private:
  const std::string name_;
  int value_{};
  int counter_ = 0;
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
                 "level": 2
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
    graph_loop_multiple_circle_through_cmd__extension, test_extension);

}  // namespace

TEST(ExtensionTest,
#if defined(__i386__) || defined(__arm__)
     // In a 32-bit environment, this test case may cause the `on_xxx` or
     // `result_handler` of the message to be called directly, instead of being
     // placed into the message queue, because all these extensions are in the
     // same extension thread. This can potentially lead to the function call
     // stack becoming too deep and resulting in a stack overflow. Therefore,
     // disable this test case in a 32-bit environment.
     DISABLED_GraphLoopMultipleCircleThroughCmd
#else
     GraphLoopMultipleCircleThroughCmd
#endif
) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr, nullptr,
                            nullptr);
  start_graph_cmd->set_graph_from_json(R"({
             "nodes": [{
               "type": "extension",
               "name": "A",
               "addon": "graph_loop_multiple_circle_through_cmd__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "graph_loop_multiple_circle_through_cmd__extension_group",
               "property": {
                 "value": 0
               }
             },{
               "type": "extension",
               "name": "B",
               "addon": "graph_loop_multiple_circle_through_cmd__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "graph_loop_multiple_circle_through_cmd__extension_group",
               "property": {
                 "value": 1
               }
             },{
               "type": "extension",
               "name": "C",
               "addon": "graph_loop_multiple_circle_through_cmd__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "graph_loop_multiple_circle_through_cmd__extension_group",
               "property": {
                 "value": 2
               }
             },{
               "type": "extension",
               "name": "D",
               "addon": "graph_loop_multiple_circle_through_cmd__extension",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "graph_loop_multiple_circle_through_cmd__extension_group",
               "property": {
                 "value": 3
               }
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "A",
               "cmd": [{
                 "name": "sum",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "B"
                 }]
               }]
             },{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "B",
               "cmd": [{
                 "name": "sum",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "C"
                 }]
               }]
             },{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "C",
               "cmd": [{
                 "name": "sum",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "D"
                 }]
               }]
             },{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "D",
               "cmd": [{
                 "name": "sum",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "B"
                 }]
               }]
             }]
         })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  auto sum_cmd = ten::cmd_t::create("sum");
  sum_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                    "graph_loop_multiple_circle_through_cmd__extension_group",
                    "A");
  cmd_result = client->send_cmd_and_recv_result(std::move(sum_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  nlohmann::json detail =
      nlohmann::json::parse(cmd_result->get_property_to_json("detail"));
  EXPECT_EQ((1 + 2 + 3) * LOOP_CNT, detail["total"].get<std::int32_t>());

  delete client;

  ten_thread_join(app_thread, -1);
}
