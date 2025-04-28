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
#include "ten_runtime/msg/cmd/stop_graph/cmd.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class extension_t;

class cmd_stop_graph_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend cmd_stop_graph_t;

    explicit ctor_passkey_t() = default;
  };

  explicit cmd_stop_graph_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<cmd_stop_graph_t> create(error_t *err = nullptr) {
    return std::make_unique<cmd_stop_graph_t>(ctor_passkey_t());
  }

  explicit cmd_stop_graph_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_stop_graph_create()) {}
  ~cmd_stop_graph_t() override = default;

  // @{
  cmd_stop_graph_t(cmd_stop_graph_t &other) = delete;
  cmd_stop_graph_t(cmd_stop_graph_t &&other) = delete;
  cmd_stop_graph_t &operator=(const cmd_stop_graph_t &cmd) = delete;
  cmd_stop_graph_t &operator=(cmd_stop_graph_t &&cmd) = delete;
  // @}

  std::string get_graph_id(error_t *err = nullptr) const {
    return ten_cmd_stop_graph_get_graph_id(c_msg);
  }
  bool set_graph_id(const char *graph_id, error_t *err = nullptr) {
    return ten_cmd_stop_graph_set_graph_id(c_msg, graph_id);
  }
};

}  // namespace ten
