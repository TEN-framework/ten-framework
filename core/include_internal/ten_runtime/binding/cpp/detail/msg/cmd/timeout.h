//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <memory>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/timeout/cmd.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/cmd.h"

namespace ten {

class extension_t;

class cmd_timeout_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend cmd_timeout_t;

    explicit ctor_passkey_t() = default;
  };

  static std::unique_ptr<cmd_timeout_t> create(ten_shared_ptr_t *cmd,
                                               error_t *err = nullptr) {
    return std::make_unique<cmd_timeout_t>(cmd, ctor_passkey_t());
  }

  explicit cmd_timeout_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  explicit cmd_timeout_t(ten_shared_ptr_t *cmd, ctor_passkey_t /*unused*/)
      : cmd_t(cmd) {}
  ~cmd_timeout_t() override = default;

  uint32_t get_timer_id(error_t *err = nullptr) const {
    return ten_cmd_timeout_get_timer_id(c_msg);
  }

  // @{
  cmd_timeout_t() = delete;
  cmd_timeout_t(cmd_timeout_t &other) = delete;
  cmd_timeout_t(cmd_timeout_t &&other) = delete;
  cmd_timeout_t &operator=(const cmd_timeout_t &cmd) = delete;
  cmd_timeout_t &operator=(cmd_timeout_t &&cmd) = delete;
  // @}
};

}  // namespace ten
