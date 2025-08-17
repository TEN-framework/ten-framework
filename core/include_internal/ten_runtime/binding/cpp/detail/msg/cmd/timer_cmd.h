//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <memory>

#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/cmd.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/cmd.h"

namespace ten {

class extension_t;

class timer_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend timer_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  explicit timer_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<timer_cmd_t> create(error_t *err = nullptr) {
    return std::make_unique<timer_cmd_t>(ctor_passkey_t());
  }

  explicit timer_cmd_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_timer_create()) {}
  ~timer_cmd_t() override = default;

  bool set_timer_id(uint32_t timer_id) {
    return ten_cmd_timer_set_timer_id(c_msg, timer_id);
  }

  bool set_times(int32_t times) {
    return ten_cmd_timer_set_times(c_msg, times);
  }

  bool set_timeout_us(int64_t timeout_us) {
    return ten_cmd_timer_set_timeout_us(c_msg, timeout_us);
  }

  // @{
  timer_cmd_t(timer_cmd_t &other) = delete;
  timer_cmd_t(timer_cmd_t &&other) = delete;
  timer_cmd_t &operator=(const timer_cmd_t &cmd) = delete;
  timer_cmd_t &operator=(timer_cmd_t &&cmd) = delete;
  // @}
};

}  // namespace ten
