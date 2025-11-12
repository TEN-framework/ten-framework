//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/binding/cpp/detail/ten_env.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"

using ten_extension_t = struct ten_extension_t;
using ten_env_proxy_t = struct ten_env_proxy_t;
using ten_env_t = struct ten_env_t;

namespace ten {

class extension_t;
class extension_group_t;
class app_t;

namespace {

using notify_std_func_t = std::function<void(ten_env_t &)>;
using notify_std_with_user_data_func_t =
    std::function<void(ten_env_t &, void *user_data)>;

struct proxy_notify_ctx_t {
  explicit proxy_notify_ctx_t(notify_std_func_t &&func)
      : notify_std_func(std::move(func)) {}
  explicit proxy_notify_ctx_t(notify_std_with_user_data_func_t &&func,
                              void *user_data)
      : notify_std_with_user_data_func(std::move(func)), user_data(user_data) {}

  ~proxy_notify_ctx_t() = default;

  // @{
  proxy_notify_ctx_t(const proxy_notify_ctx_t &) = delete;
  proxy_notify_ctx_t &operator=(const proxy_notify_ctx_t &) = delete;
  proxy_notify_ctx_t(proxy_notify_ctx_t &&) = delete;
  proxy_notify_ctx_t &operator=(proxy_notify_ctx_t &&) = delete;
  // @}

  notify_std_func_t notify_std_func;

  notify_std_with_user_data_func_t notify_std_with_user_data_func;
  void *user_data{};
};

inline void proxy_notify(::ten_env_t *ten_env, void *data = nullptr) {
  TEN_ASSERT(data, "Invalid argument.");

  auto *ctx = static_cast<proxy_notify_ctx_t *>(data);
  auto *cpp_ten_env =
      static_cast<ten_env_t *>(ten_binding_handle_get_me_in_target_lang(
          reinterpret_cast<ten_binding_handle_t *>(ten_env)));

  if (ctx->notify_std_func != nullptr) {
    auto func = ctx->notify_std_func;
    func(*cpp_ten_env);
  } else if (ctx->notify_std_with_user_data_func != nullptr) {
    auto func = ctx->notify_std_with_user_data_func;
    func(*cpp_ten_env, ctx->user_data);
  }

  delete ctx;
}

}  // namespace

class ten_env_proxy_t {
 private:
  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend ten_env_proxy_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  static ten_env_proxy_t *create(ten_env_t &ten_env, error_t *err = nullptr) {
    return new ten_env_proxy_t(ten_env, ctor_passkey_t());
  }

  explicit ten_env_proxy_t(ten_env_t &ten_env, ctor_passkey_t /*unused*/)
      : c_ten_env_proxy(ten_env_proxy_create(ten_env.c_ten_env, 1, nullptr)) {}

  ~ten_env_proxy_t() {
    if (c_ten_env_proxy == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
    }

    bool rc = ten_env_proxy_release(c_ten_env_proxy, nullptr);
    TEN_ASSERT(rc, "Should not happen.");
  };

  // @{
  ten_env_proxy_t(ten_env_proxy_t &other) = delete;
  ten_env_proxy_t(ten_env_proxy_t &&other) = delete;
  ten_env_proxy_t &operator=(const ten_env_proxy_t &other) = delete;
  ten_env_proxy_t &operator=(ten_env_proxy_t &&other) = delete;
  // @}

  bool acquire_lock_mode(error_t *err = nullptr) {
    if (c_ten_env_proxy == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
      return false;
    }

    return ten_env_proxy_acquire_lock_mode(
        c_ten_env_proxy, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool release_lock_mode(error_t *err = nullptr) {
    if (c_ten_env_proxy == nullptr) {
      TEN_ASSERT(0, "Invalid argument.");
      return false;
    }

    return ten_env_proxy_release_lock_mode(
        c_ten_env_proxy, err != nullptr ? err->get_c_error() : nullptr);
  }

  bool notify(notify_std_func_t &&notify_func, bool sync = false,
              error_t *err = nullptr) {
    auto *ctx = new proxy_notify_ctx_t(std::move(notify_func));

    auto rc =
        ten_env_proxy_notify(c_ten_env_proxy, proxy_notify, ctx, sync,
                             err != nullptr ? err->get_c_error() : nullptr);
    if (!rc) {
      delete ctx;
    }

    return rc;
  }

  bool notify(notify_std_with_user_data_func_t &&notify_func, void *user_data,
              bool sync = false, error_t *err = nullptr) {
    auto *info = new proxy_notify_ctx_t(std::move(notify_func), user_data);

    auto rc =
        ten_env_proxy_notify(c_ten_env_proxy, proxy_notify, info, sync,
                             err != nullptr ? err->get_c_error() : nullptr);
    if (!rc) {
      delete info;
    }

    return rc;
  }

 private:
  ::ten_env_proxy_t *c_ten_env_proxy;

  // We do not provide the following API. If having similar needs, it can be
  // achieved by creating a new ten_env_proxy.
  //
  // bool acquire(error_t *err = nullptr) {
  //   if (c_ten_env_proxy == nullptr) {
  //     TEN_ASSERT(0, "Invalid argument.");
  //     return false;
  //   }
  //   return ten_proxy_acquire(
  //       c_ten_env_proxy,
  //       err != nullptr ? err->get_c_error() : nullptr);
  // }
  //
  // bool release(error_t *err = nullptr) {
  //   if (c_ten_env_proxy == nullptr) {
  //     TEN_ASSERT(0, "Invalid argument.");
  //     return false;
  //   }
  //   bool rc = ten_proxy_release(
  //       c_ten_env_proxy,
  //       err != nullptr ? err->get_c_error() : nullptr);
  //   TEN_ASSERT(rc, "Should not happen.");
  //   return rc;
  // }
};

}  // namespace ten
