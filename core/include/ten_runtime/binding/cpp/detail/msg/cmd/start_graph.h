//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <memory>

#include "ten_runtime/binding/cpp/detail/msg/cmd/cmd.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"

namespace ten {

class extension_t;

class cmd_start_graph_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend cmd_start_graph_t;

    explicit ctor_passkey_t() = default;
  };

  explicit cmd_start_graph_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<cmd_start_graph_t> create(error_t *err = nullptr) {
    return std::make_unique<cmd_start_graph_t>(ctor_passkey_t());
  }

  explicit cmd_start_graph_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_start_graph_create()) {};
  ~cmd_start_graph_t() override = default;

  bool set_predefined_graph_name(const char *predefined_graph_name,
                                 error_t *err = nullptr) {
    return ten_cmd_start_graph_set_predefined_graph_name(
        c_msg, predefined_graph_name,
        err != nullptr ? err->get_c_error() : nullptr);
  }

  bool set_graph_from_json(const char *json_str, error_t *err = nullptr) {
    return ten_cmd_start_graph_set_graph_from_json_str(
        c_msg, json_str, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool set_long_running_mode(bool long_running_mode, error_t *err = nullptr) {
    return ten_cmd_start_graph_set_long_running_mode(
        c_msg, long_running_mode,
        err != nullptr ? err->get_c_error() : nullptr);
  }

  // @{
  cmd_start_graph_t(cmd_start_graph_t &other) = delete;
  cmd_start_graph_t(cmd_start_graph_t &&other) = delete;
  cmd_start_graph_t &operator=(const cmd_start_graph_t &cmd) = delete;
  cmd_start_graph_t &operator=(cmd_start_graph_t &&cmd) = delete;
  // @}
};

}  // namespace ten
