//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "ten_runtime/binding/cpp/detail/msg/cmd/start_graph.h"
#include "ten_utils/macro/check.h"
#include "tests/common/client/cpp/msgpack_tcp.h"

namespace {

void test_extension_in_app1_not_installed() {
  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send a start_graph cmd to app 8001. However, because there is no extension
  // addon named `ext_e` in app 8001, the `start_graph` command will fail.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
               "type": "extension",
               "name": "ext_a",
               "addon": "ext_e",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "test_extension_group"
             }, {
               "type": "extension",
               "name": "ext_b",
               "addon": "ext_b",
               "app": "msgpack://127.0.0.1:8002/",
               "extension_group": "test_extension_group"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_ERROR == cmd_result->get_status_code(),
             "Should not happen.");

  delete client;
}

void test_extension_in_app2_not_installed() {
  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send a start_graph cmd to app 8001. However, because there is no extension
  // addon named `ext_e` in app 8002, the `start_graph` command will fail.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(
      R"({
             "nodes": [{
               "type": "extension",
               "name": "ext_a",
               "addon": "ext_a",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "test_extension_group"
             }, {
               "type": "extension",
               "name": "ext_b",
               "addon": "ext_e",
               "app": "msgpack://127.0.0.1:8002/",
               "extension_group": "test_extension_group"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "ext_a",
               "cmd": [{
                 "name": "hello_world",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8002/",
                   "extension": "ext_b"
                 }]
               }]
             }]
         })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_ERROR == cmd_result->get_status_code(),
             "Should not happen.");

  delete client;
}

}  // namespace

int main(int argc, char **argv) {
  test_extension_in_app1_not_installed();
  test_extension_in_app2_not_installed();

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(
      R"({
             "nodes": [{
               "type": "extension",
               "name": "ext_a",
               "addon": "ext_a",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "test_extension_group"
             }]
         })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "test_extension_group", "ext_a");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");
  TEN_ASSERT(static_cast<std::string>("hello world, too") ==
                 cmd_result->get_property_string("detail"),
             "Should not happen.");

  client->close_app();

  delete client;

  auto *client2 = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8002/");
  client2->close_app();
  delete client2;
}
