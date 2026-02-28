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

class stop_graph_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend stop_graph_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  explicit stop_graph_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<stop_graph_cmd_t> create(error_t *err = nullptr) {
    return std::make_unique<stop_graph_cmd_t>(ctor_passkey_t());
  }

  explicit stop_graph_cmd_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_stop_graph_create()) {}
  ~stop_graph_cmd_t() override = default;

  // @{
  stop_graph_cmd_t(stop_graph_cmd_t &other) = delete;
  stop_graph_cmd_t(stop_graph_cmd_t &&other) = delete;
  stop_graph_cmd_t &operator=(const stop_graph_cmd_t &cmd) = delete;
  stop_graph_cmd_t &operator=(stop_graph_cmd_t &&cmd) = delete;
  // @}

  std::string get_graph_id(error_t *err = nullptr) const {
    return ten_cmd_stop_graph_get_graph_id(c_msg);
  }
  bool set_graph_id(const char *graph_id, error_t *err = nullptr) {
    return ten_cmd_stop_graph_set_graph_id(c_msg, graph_id);
  }
};

}  // namespace ten
