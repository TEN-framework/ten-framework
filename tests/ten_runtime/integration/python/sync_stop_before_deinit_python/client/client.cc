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

  // Send a "test" command to test_extension_1 (predefined "default" graph).
  // test_extension_1 replies only after:
  //   1. stop_graph is complete for the dynamic graph, AND
  //   2. the "on_stop_notify" from test_extension_2 has been received.
  // With sync_stop_before_deinit, stop_graph only completes after BOTH
  // extensions have called on_stop_done, so the notification is guaranteed to
  // be in-flight before stop_graph finishes.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "test_extension_1"}});

  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));

  auto status_code = cmd_result->get_status_code();
  if (status_code == TEN_STATUS_CODE_OK) {
    auto detail = cmd_result->get_property_string("detail");
    TEN_ASSERT(detail == std::string("{\"id\": 1, \"name\": \"a\"}"),
               "Should not happen.");
    std::cout << "Received: " << detail << '\n';
  } else {
    std::cout << "Command failed with status: " << status_code << '\n';
    delete client;
    return 1;
  }

  delete client;

  return 0;
}
