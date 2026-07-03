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

class timeout_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend timeout_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  static std::unique_ptr<timeout_cmd_t> create(ten_shared_ptr_t *cmd,
                                               error_t *err = nullptr) {
    return std::make_unique<timeout_cmd_t>(cmd, ctor_passkey_t());
  }

  explicit timeout_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  explicit timeout_cmd_t(ten_shared_ptr_t *cmd, ctor_passkey_t /*unused*/)
      : cmd_t(cmd) {}
  ~timeout_cmd_t() override = default;

  uint32_t get_timer_id(error_t *err = nullptr) const {
    return ten_cmd_timeout_get_timer_id(c_msg);
  }

  // @{
  timeout_cmd_t() = delete;
  timeout_cmd_t(timeout_cmd_t &other) = delete;
  timeout_cmd_t(timeout_cmd_t &&other) = delete;
  timeout_cmd_t &operator=(const timeout_cmd_t &cmd) = delete;
  timeout_cmd_t &operator=(timeout_cmd_t &&cmd) = delete;
  // @}
};

}  // namespace ten
