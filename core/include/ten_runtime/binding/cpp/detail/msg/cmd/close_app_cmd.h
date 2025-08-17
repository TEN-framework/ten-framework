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

class close_app_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend close_app_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  explicit close_app_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<close_app_cmd_t> create(error_t *err = nullptr) {
    return std::make_unique<close_app_cmd_t>(ctor_passkey_t());
  }

  explicit close_app_cmd_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_close_app_create()) {}

  ~close_app_cmd_t() override = default;

  // @{
  close_app_cmd_t(ten_cmd_close_app_t *cmd) = delete;
  close_app_cmd_t(close_app_cmd_t &other) = delete;
  close_app_cmd_t(close_app_cmd_t &&other) = delete;
  close_app_cmd_t &operator=(const close_app_cmd_t &cmd) = delete;
  close_app_cmd_t &operator=(close_app_cmd_t &&cmd) = delete;
  // @}
};

}  // namespace ten
