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

  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dest("msgpack://127.0.0.1:8007/", "default", "nodetest", "A");
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  TEN_ASSERT(TEN_STATUS_CODE_OK == cmd_result->get_status_code(),
             "Should not happen.");

  TEN_LOGD("Got graph result.");

  std::string detail_str = cmd_result->get_property_string("detail");
  TEN_LOGD("Got result: %s", detail_str.c_str());
  TEN_ASSERT(detail_str == std::string("okok"), "Should not happen.");

  // NOTE the order: client destroy, then connection lost, then nodejs exits.
  delete client;
}
