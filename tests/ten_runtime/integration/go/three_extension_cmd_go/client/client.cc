//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "include_internal/ten_runtime/binding/cpp/ten.h"
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
          "extension_group": "nodetest_group",
          "addon": "extension_a",
          "name": "A"
        },
        {
          "type": "extension",
          "app": "msgpack://127.0.0.1:8007/",
          "extension_group": "nodetest_group",
          "addon": "extension_b",
          "name": "B"
        },
        {
          "type": "extension",
          "app": "msgpack://127.0.0.1:8007/",
          "extension_group": "nodetest_group",
          "addon": "extension_c",
          "name": "C"
        }
      ],
      "connections": [
        {
          "app": "msgpack://127.0.0.1:8007/",
          "extension": "A",
          "cmd": [{
            "name": "B",
            "dest": [{
              "app": "msgpack://127.0.0.1:8007/",
              "extension": "B"
            }]
          }]
        },
        {
          "app": "msgpack://127.0.0.1:8007/",
          "extension": "B",
          "cmd": [{
            "name": "C",
            "dest": [{
              "app": "msgpack://127.0.0.1:8007/",
              "extension": "C"
            }]
          }]
        }
      ]
    })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  TEN_LOGD("client sent json");

  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  TEN_LOGD("got graph result");
  auto A_cmd = ten::cmd_t::create("A");
  A_cmd->set_dest("msgpack://127.0.0.1:8007/", nullptr, "nodetest_group", "A");
  cmd_result = client->send_cmd_and_recv_result(std::move(A_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  std::string detail_str = cmd_result->get_property_string("detail");
  TEN_LOGD("got result: %s", detail_str.c_str());

  ten_json_t *detail_json = ten_json_from_string(detail_str.c_str(), nullptr);

  ten_json_t result_json = TEN_JSON_INIT_VAL(detail_json->ctx, false);

  bool success = ten_json_object_peek(detail_json, "result", &result_json);
  TEN_ASSERT(success, "Should not happen.");

  TEN_ASSERT(30 == ten_json_get_integer_value(&result_json),
             "Should not happen.");

  ten_json_deinit(&result_json);
  ten_json_destroy(detail_json);

  // NOTE the order: client destroy, then connection lost, then nodejs exits
  delete client;
}
