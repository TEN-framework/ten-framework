//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <memory>
#include <string>

#include "ten_utils/io/runloop.h"

namespace ten {

class Runloop;
using TenRunloop = std::unique_ptr<Runloop>;

class Runloop {
 public:
  static TenRunloop Create(const std::string &impl = "") {
    auto loop = ten_runloop_create(impl.empty() ? nullptr : impl.c_str());
    if (!loop) {
      return nullptr;
    }

    return std::unique_ptr<Runloop>(new (std::nothrow) Runloop(loop));
  }

  Runloop() = delete;

  Runloop(const Runloop &rhs) = delete;
  Runloop &operator=(const Runloop &rhs) = delete;

  Runloop(Runloop &&rhs) = delete;

  Runloop &operator=(Runloop &&rhs) = delete;

  ~Runloop() { ten_runloop_destroy(loop_); }

 public:
  ::ten_runloop_t *get_c_loop() const { return loop_; }

  void Run() { ten_runloop_run(loop_); }

  void Stop() { ten_runloop_stop(loop_); }

  bool Alive() const { return ten_runloop_alive(loop_) == 1; }

 private:
  explicit Runloop(::ten_runloop_t *loop) : loop_(loop) {}

 private:
  ::ten_runloop_t *loop_;
};

}  // namespace ten
