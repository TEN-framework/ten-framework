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
#include "ten_runtime/msg/cmd/close_app/cmd.h"

namespace ten {

class extension_t;

class cmd_close_app_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend cmd_close_app_t;

    explicit ctor_passkey_t() = default;
  };

  explicit cmd_close_app_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<cmd_close_app_t> create(error_t *err = nullptr) {
    return std::make_unique<cmd_close_app_t>(ctor_passkey_t());
  }

  explicit cmd_close_app_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_close_app_create()) {}

  ~cmd_close_app_t() override = default;

  // @{
  cmd_close_app_t(ten_cmd_close_app_t *cmd) = delete;
  cmd_close_app_t(cmd_close_app_t &other) = delete;
  cmd_close_app_t(cmd_close_app_t &&other) = delete;
  cmd_close_app_t &operator=(const cmd_close_app_t &cmd) = delete;
  cmd_close_app_t &operator=(cmd_close_app_t &&cmd) = delete;
  // @}
};

}  // namespace ten
