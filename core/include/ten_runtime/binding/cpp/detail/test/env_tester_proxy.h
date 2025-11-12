
//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <functional>

#include "ten_runtime/binding/cpp/detail/test/env_tester.h"
#include "ten_runtime/test/env_tester_proxy.h"

using ten_env_tester_t = struct ten_env_tester_t;
using ten_env_tester_proxy_t = struct ten_env_tester_proxy_t;

namespace ten {

namespace {

using tester_notify_std_func_t = std::function<void(ten_env_tester_t &)>;

struct tester_proxy_notify_ctx_t {
  explicit tester_proxy_notify_ctx_t(tester_notify_std_func_t &&func)
      : notify_std_func(std::move(func)) {}

  ~tester_proxy_notify_ctx_t() = default;

  // @{
  tester_proxy_notify_ctx_t(const tester_proxy_notify_ctx_t &) = delete;
  tester_proxy_notify_ctx_t &operator=(const tester_proxy_notify_ctx_t &) =
      delete;
  tester_proxy_notify_ctx_t(tester_proxy_notify_ctx_t &&) = delete;
  tester_proxy_notify_ctx_t &operator=(tester_proxy_notify_ctx_t &&) = delete;
  // @}

  tester_notify_std_func_t notify_std_func;
};

inline void proxy_notify(::ten_env_tester_t *ten_env, void *data = nullptr) {
  TEN_ASSERT(data, "Invalid argument.");

  auto *ctx = static_cast<tester_proxy_notify_ctx_t *>(data);
  auto *cpp_ten_env =
      static_cast<ten_env_tester_t *>(ten_binding_handle_get_me_in_target_lang(
          reinterpret_cast<ten_binding_handle_t *>(ten_env)));

  if (ctx->notify_std_func != nullptr) {
    auto func = ctx->notify_std_func;
    func(*cpp_ten_env);
  }

  delete ctx;
}

}  // namespace

class ten_env_tester_proxy_t {
 private:
  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend ten_env_tester_proxy_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  // @{
  ten_env_tester_proxy_t(const ten_env_tester_proxy_t &) = delete;
  ten_env_tester_proxy_t(ten_env_tester_proxy_t &&) = delete;
  ten_env_tester_proxy_t &operator=(const ten_env_tester_proxy_t &) = delete;
  ten_env_tester_proxy_t &operator=(const ten_env_tester_proxy_t &&) = delete;
  // @{

  static ten_env_tester_proxy_t *create(ten_env_tester_t &ten_env_tester,
                                        error_t *err = nullptr) {
    return new ten_env_tester_proxy_t(ten_env_tester, ctor_passkey_t());
  }

  explicit ten_env_tester_proxy_t(ten_env_tester_t &ten_env_tester,
                                  ctor_passkey_t /*unused*/)
      : c_ten_env_tester_proxy(ten_env_tester_proxy_create(
            ten_env_tester.c_ten_env_tester, nullptr)) {}

  ~ten_env_tester_proxy_t() {
    if (c_ten_env_tester_proxy == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
    }

    bool rc = ten_env_tester_proxy_release(c_ten_env_tester_proxy, nullptr);
    TEN_ASSERT(rc, "Should not happen.");

    c_ten_env_tester_proxy = nullptr;
  };

  bool notify(tester_notify_std_func_t &&notify_func, error_t *err = nullptr) {
    if (c_ten_env_tester_proxy == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
      return false;
    }

    auto *ctx = new tester_proxy_notify_ctx_t(std::move(notify_func));

    auto rc = ten_env_tester_proxy_notify(
        c_ten_env_tester_proxy, proxy_notify, ctx,
        err != nullptr ? err->get_c_error() : nullptr);
    if (!rc) {
      delete ctx;
    }

    return rc;
  }

 private:
  ::ten_env_tester_proxy_t *c_ten_env_tester_proxy;
};
}  // namespace ten