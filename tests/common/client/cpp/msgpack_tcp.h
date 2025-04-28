//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <cstddef>
#include <exception>
#include <nlohmann/json.hpp>

#include "include_internal/ten_runtime/binding/cpp/detail/msg/cmd/cmd_result_internal_accessor.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd_result.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "tests/common/client/msgpack_tcp.h"

namespace ten {

class msgpack_tcp_client_t {
 public:
  explicit msgpack_tcp_client_t(const char *app_id)
      : c_client(ten_test_msgpack_tcp_client_create(app_id)) {
    if (c_client == nullptr) {
      throw std::exception();
    }
  }

  ~msgpack_tcp_client_t() { ten_test_msgpack_tcp_client_destroy(c_client); }

  msgpack_tcp_client_t(const msgpack_tcp_client_t &) = delete;
  msgpack_tcp_client_t &operator=(const msgpack_tcp_client_t &) = delete;

  msgpack_tcp_client_t(msgpack_tcp_client_t &&) = delete;
  msgpack_tcp_client_t &operator=(msgpack_tcp_client_t &&) = delete;

  bool send_cmd(std::unique_ptr<ten::cmd_t> &&cmd) {
    bool success = ten_test_msgpack_tcp_client_send_msg(
        c_client, cmd->get_underlying_msg());
    if (success) {
      // Only when the cmd has been sent successfully, we should give back the
      // ownership of the cmd to the TEN runtime.
      auto *cpp_cmd_ptr = cmd.release();
      delete cpp_cmd_ptr;
    }
    return success;
  }

  std::unique_ptr<ten::cmd_result_t> send_cmd_and_recv_result(
      std::unique_ptr<ten::cmd_t> &&cmd) {
    send_cmd(std::move(cmd));

    ten_shared_ptr_t *c_resp = ten_test_msgpack_tcp_client_recv_msg(c_client);
    if (c_resp != nullptr) {
      return ten::cmd_result_internal_accessor_t::create(c_resp);
    } else {
      return {};
    }
  }

  std::vector<std::unique_ptr<ten::cmd_result_t>> batch_recv_cmd_results() {
    ten_list_t msgs = TEN_LIST_INIT_VAL;
    ten_test_msgpack_tcp_client_recv_msgs_batch(c_client, &msgs);

    std::vector<std::unique_ptr<ten::cmd_result_t>> results;

    ten_list_foreach (&msgs, iter) {
      ten_shared_ptr_t *c_cmd_result =
          ten_shared_ptr_clone(ten_smart_ptr_listnode_get(iter.node));
      TEN_ASSERT(c_cmd_result, "Should not happen.");

      auto cmd_result =
          ten::cmd_result_internal_accessor_t::create(c_cmd_result);
      results.push_back(std::move(cmd_result));
    }

    ten_list_clear(&msgs);

    return results;
  }

  bool send_data(const std::string &graph_id,
                 const std::string &extension_group_name,
                 const std::string &extension_name, void *data, size_t size) {
    return ten_test_msgpack_tcp_client_send_data(
        c_client, graph_id.c_str(), extension_group_name.c_str(),
        extension_name.c_str(), data, size);
  }

  bool close_app() {
    bool rc = ten_test_msgpack_tcp_client_close_app(c_client);
    TEN_ASSERT(rc, "Should not happen.");

    return rc;
  }

  static bool close_app(const std::string &app_uri) {
    auto *client = new msgpack_tcp_client_t(app_uri.c_str());
    bool rc = ten_test_msgpack_tcp_client_close_app(client->c_client);
    delete client;

    TEN_ASSERT(rc, "Should not happen.");
    return rc;
  }

  void get_info(std::string &ip, uint16_t &port) {
    ten_string_t c_ip;
    TEN_STRING_INIT(c_ip);
    ten_test_msgpack_tcp_client_get_info(c_client, &c_ip, &port);

    ip = ten_string_get_raw_str(&c_ip);

    ten_string_deinit(&c_ip);
  }

 private:
  ten_test_msgpack_tcp_client_t *c_client;
};

}  // namespace ten
