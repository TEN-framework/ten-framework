//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/macro/check.h"

namespace ten {

class binding_handle_t {
 protected:
  explicit binding_handle_t(void *c_instance) : c_instance(c_instance) {
    TEN_ASSERT(c_instance, "Should not happen.");
  }

 public:
  virtual ~binding_handle_t() = default;

  // @{
  binding_handle_t(const binding_handle_t &) = delete;
  binding_handle_t(binding_handle_t &&) = delete;
  binding_handle_t &operator=(const binding_handle_t &) = delete;
  binding_handle_t &operator=(const binding_handle_t &&) = delete;
  // @}

  void *get_c_instance() const { return c_instance; }

 private:
  void *c_instance = nullptr;
};

}  // namespace ten
