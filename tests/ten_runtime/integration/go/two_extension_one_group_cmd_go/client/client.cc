//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "ten_utils/macro/mark.h"
#include "tests/common/client/cpp/msgpack_tcp.h"

int main(TEN_UNUSED int argc, TEN_UNUSED char **argv) {
  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8007/");

  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [
          {
            "type": "extension",
            "app": "msgpack://127.0.0.1:8007/",
            "extension_group": "nodetest",
            "addon": "addon_a",
            "name": "A"
          },
          {
            "type": "extension",
            "app": "msgpack://127.0.0.1:8007/",
            "extension_group": "nodetest",
            "addon": "addon_b",
            "name": "B"
          }
        ],
        "connections": [
          {
            "app": "msgpack://127.0.0.1:8007/",
            "extension": "A",
            "data": [{
              "name": "data",
              "dest": [{
                "app": "msgpack://127.0.0.1:8007/",
                "extension": "B"
              }]
            }]
          }
        ]
      })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  TEN_LOGD("Got graph result.");
  auto A_cmd = ten::cmd_t::create("A");
  A_cmd->set_dest("msgpack://127.0.0.1:8007/", nullptr, "nodetest", "A");
  cmd_result = client->send_cmd_and_recv_result(std::move(A_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  std::string resp_str = cmd_result->get_property_string("detail");
  TEN_LOGD("Got result: %s", resp_str.c_str());
  TEN_ASSERT(resp_str == std::string("world"), "Should not happen.");

  // NOTE the order: client destroy, then connection lost, then go app exits.
  delete client;
}
