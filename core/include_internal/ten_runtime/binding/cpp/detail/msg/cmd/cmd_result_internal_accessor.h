//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/binding/cpp/detail/msg/cmd_result.h"

namespace ten {

class cmd_result_internal_accessor_t {
 public:
  explicit cmd_result_internal_accessor_t() = default;
  ~cmd_result_internal_accessor_t() = default;

  // @{
  cmd_result_internal_accessor_t(const cmd_result_internal_accessor_t &) =
      delete;
  cmd_result_internal_accessor_t(cmd_result_internal_accessor_t &&) = delete;
  cmd_result_internal_accessor_t &operator=(
      const cmd_result_internal_accessor_t &) = delete;
  cmd_result_internal_accessor_t &operator=(cmd_result_internal_accessor_t &&) =
      delete;
  // @}

  static std::unique_ptr<cmd_result_t> create(ten_shared_ptr_t *cmd,
                                              error_t *err = nullptr) {
    return ten::cmd_result_t::create(cmd, err);
  }
};

}  // namespace ten
