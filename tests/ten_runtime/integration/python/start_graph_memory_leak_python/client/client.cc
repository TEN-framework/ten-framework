//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <iostream>

#include "tests/common/client/cpp/msgpack_tcp.h"

int main() {
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "test_extension_1"}});

  // The test may take a while (multiple start/stop graph cycles), so just wait
  // for the result without a strict timeout.
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));

  auto status_code = cmd_result->get_status_code();
  if (status_code != TEN_STATUS_CODE_OK) {
    auto detail = cmd_result->get_property_string("detail");
    std::cerr << "Memory leak test FAILED: " << detail << std::endl;
    delete client;
    return 1;
  }

  auto detail = cmd_result->get_property_string("detail");
  std::cout << "Memory leak test PASSED: " << detail << std::endl;

  delete client;
  return 0;
}
