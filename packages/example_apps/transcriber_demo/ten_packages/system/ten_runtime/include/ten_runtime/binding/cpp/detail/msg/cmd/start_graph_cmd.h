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

class start_graph_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend start_graph_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  explicit start_graph_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<start_graph_cmd_t> create(error_t *err = nullptr) {
    return std::make_unique<start_graph_cmd_t>(ctor_passkey_t());
  }

  explicit start_graph_cmd_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_start_graph_create()) {};
  ~start_graph_cmd_t() override = default;

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
  start_graph_cmd_t(start_graph_cmd_t &other) = delete;
  start_graph_cmd_t(start_graph_cmd_t &&other) = delete;
  start_graph_cmd_t &operator=(const start_graph_cmd_t &cmd) = delete;
  start_graph_cmd_t &operator=(start_graph_cmd_t &&cmd) = delete;
  // @}
};

}  // namespace ten
