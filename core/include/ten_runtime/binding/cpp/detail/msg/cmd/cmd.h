//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <memory>

#include "ten_runtime/binding/cpp/detail/msg/msg.h"
#include "ten_runtime/msg/cmd/cmd.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class extension_t;
class extension_tester_t;
class ten_env_t;

class cmd_t : public msg_t {
 private:
  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend cmd_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  static std::unique_ptr<cmd_t> create(const char *name,
                                       error_t *err = nullptr) {
    ten_shared_ptr_t *c_cmd =
        ten_cmd_create(name, err != nullptr ? err->get_c_error() : nullptr);

    return std::make_unique<cmd_t>(c_cmd, ctor_passkey_t());
  }

  explicit cmd_t(ten_shared_ptr_t *cmd, ctor_passkey_t /*unused*/)
      : msg_t(cmd) {}

  cmd_t() = default;
  ~cmd_t() override = default;

  // @{
  cmd_t(const cmd_t &other) noexcept = delete;
  cmd_t(cmd_t &&other) noexcept = delete;
  cmd_t &operator=(const cmd_t &cmd) noexcept = delete;
  cmd_t &operator=(cmd_t &&cmd) noexcept = delete;
  // @}

  std::unique_ptr<cmd_t> clone() const {
    if (c_msg == nullptr) {
      TEN_ASSERT(0, "Should not happen.");
      return nullptr;
    }

    ten_shared_ptr_t *cloned_msg = ten_msg_clone(c_msg, nullptr);
    if (cloned_msg == nullptr) {
      return nullptr;
    }

    return std::make_unique<cmd_t>(cloned_msg, ctor_passkey_t());
  }

 protected:
  // @{
  // Used by the constructor of the real command class to create a base command
  // first.
  explicit cmd_t(ten_shared_ptr_t *cmd) : msg_t(cmd) {}
  // @}

 private:
  friend extension_t;
  friend extension_tester_t;
  friend ten_env_t;

  static std::unique_ptr<cmd_t> create(ten_shared_ptr_t *cmd,
                                       error_t *err = nullptr) {
    return std::make_unique<cmd_t>(cmd, ctor_passkey_t());
  }
};

}  // namespace ten
