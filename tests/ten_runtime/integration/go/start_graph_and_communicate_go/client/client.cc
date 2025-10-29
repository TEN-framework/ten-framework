//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <iostream>
#include <nlohmann/json.hpp>

#include "tests/common/client/cpp/msgpack_tcp.h"

int main() {
  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // The 'graph_id' MUST be "default" (a special string) if we want to send the
  // request to predefined graph.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "test_extension_1"}});

  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));

  // Parse the result and display
  auto status_code = cmd_result->get_status_code();
  if (status_code == TEN_STATUS_CODE_OK) {
    auto detail = cmd_result->get_property_string("detail");
    TEN_ASSERT(detail == std::string("{\"id\":1,\"name\":\"a\"}"),
               "Should not happen.");
    std::cout << "Received: " << detail << '\n';
  } else {
    std::cout << "Command failed with status: " << status_code << '\n';
  }

  delete client;

  return 0;
}
