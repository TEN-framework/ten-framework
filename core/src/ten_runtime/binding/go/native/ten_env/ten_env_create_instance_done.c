//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/extension/extension.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_go_ten_env_on_create_instance_done_ctx_t {
  ten_addon_host_t *addon_host;
  void *instance;
  void *context;
} ten_go_ten_env_on_create_instance_done_ctx_t;

static void ten_app_addon_host_on_create_instance_done(void *from, void *args) {
  ten_app_t *app = (ten_app_t *)from;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_go_ten_env_on_create_instance_done_ctx_t *ctx =
      (ten_go_ten_env_on_create_instance_done_ctx_t *)args;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_addon_host_t *addon_host = ctx->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_create_instance_done(addon_host->ten_env, ctx->instance,
                                            ctx->context, &err);
  if (!rc) {
    TEN_LOGE("ten_env.on_create_instance_done() in go binding failed: %s",
             ten_error_message(&err));
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_deinit(&err);

  TEN_FREE(ctx);
}

void ten_go_ten_env_on_create_instance_done(uintptr_t bridge_addr,
                                            uintptr_t instance_bridge_addr,
                                            uintptr_t context_addr) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(context_addr, "Invalid argument.");

  ten_extension_t *c_extension = NULL;
  if (instance_bridge_addr) {
    ten_go_extension_t *extension_bridge =
        ten_go_extension_reinterpret(instance_bridge_addr);
    TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
               "Should not happen.");
    c_extension = ten_go_extension_c_extension(extension_bridge);
  }

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {});

  ten_env_t *c_ten_env = self->c_ten_env;
  TEN_ASSERT(c_ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(c_ten_env, false), "Should not happen.");

  TEN_ASSERT(c_ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(c_ten_env);
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, false),
             "Should not happen.");

  ten_app_t *app = addon_host->attached_app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, false), "Should not happen.");

  ten_go_ten_env_on_create_instance_done_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_go_ten_env_on_create_instance_done_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_host = addon_host;
  ctx->instance = c_extension;
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  ctx->context = (void *)context_addr;

  int post_task_rc = ten_runloop_post_task_tail(
      ten_app_get_attached_runloop(app),
      ten_app_addon_host_on_create_instance_done, app, ctx);
  TEN_ASSERT(post_task_rc == 0, "Failed to post task.");

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return;
}
