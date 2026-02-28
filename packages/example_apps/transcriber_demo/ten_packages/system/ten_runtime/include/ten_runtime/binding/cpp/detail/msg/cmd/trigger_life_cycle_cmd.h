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
#include "ten_runtime/msg/cmd/trigger_life_cycle/cmd.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class extension_t;

class trigger_life_cycle_cmd_t : public cmd_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend trigger_life_cycle_cmd_t;

    explicit ctor_passkey_t() = default;
  };

  explicit trigger_life_cycle_cmd_t(ten_shared_ptr_t *cmd) : cmd_t(cmd) {}

 public:
  static std::unique_ptr<trigger_life_cycle_cmd_t> create(
      error_t *err = nullptr) {
    return std::make_unique<trigger_life_cycle_cmd_t>(ctor_passkey_t());
  }

  explicit trigger_life_cycle_cmd_t(ctor_passkey_t /*unused*/)
      : cmd_t(ten_cmd_trigger_life_cycle_create()) {}
  ~trigger_life_cycle_cmd_t() override = default;

  // @{
  trigger_life_cycle_cmd_t(trigger_life_cycle_cmd_t &other) = delete;
  trigger_life_cycle_cmd_t(trigger_life_cycle_cmd_t &&other) = delete;
  trigger_life_cycle_cmd_t &operator=(const trigger_life_cycle_cmd_t &cmd) =
      delete;
  trigger_life_cycle_cmd_t &operator=(trigger_life_cycle_cmd_t &&cmd) = delete;
  // @}

  std::string get_stage(error_t *err = nullptr) const {
    return ten_cmd_trigger_life_cycle_get_stage(c_msg);
  }

  bool set_stage(const char *stage, error_t *err = nullptr) {
    return ten_cmd_trigger_life_cycle_set_stage(c_msg, stage);
  }
};

}  // namespace ten
