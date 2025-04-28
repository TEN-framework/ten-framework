//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "tests/common/client/cpp/msgpack_tcp.h"

int main(int argc, char **argv) {
  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
               "type": "extension",
               "name": "test_extension",
               "addon": "default_extension_cpp",
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
                            "test_extension_group", "test_extension");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");
  TEN_ASSERT(static_cast<std::string>("hello world, too") ==
                 cmd_result->get_property_string("detail"),
             "Should not happen.");

  delete client;
}
