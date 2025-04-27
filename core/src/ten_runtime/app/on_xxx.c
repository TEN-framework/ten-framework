//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/on_xxx.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

ten_app_thread_on_addon_create_protocol_done_ctx_t *
ten_app_thread_on_addon_create_protocol_done_ctx_create(void) {
  ten_app_thread_on_addon_create_protocol_done_ctx_t *self =
      TEN_MALLOC(sizeof(ten_app_thread_on_addon_create_protocol_done_ctx_t));

  self->protocol = NULL;
  self->addon_context = NULL;

  return self;
}

static void ten_app_thread_on_addon_create_protocol_done_ctx_destroy(
    ten_app_thread_on_addon_create_protocol_done_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}

void ten_app_thread_on_addon_create_protocol_done(void *self, void *arg) {
  ten_app_t *app = (ten_app_t *)self;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_app_thread_on_addon_create_protocol_done_ctx_t *ctx =
      (ten_app_thread_on_addon_create_protocol_done_ctx_t *)arg;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_protocol_t *protocol = ctx->protocol;
  ten_addon_context_t *addon_context = ctx->addon_context;

  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &protocol->thread_check);
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "Should not happen.");

  TEN_ASSERT(addon_context, "Invalid argument.");

  if (addon_context->create_instance_done_cb) {
    addon_context->create_instance_done_cb(
        app->ten_env, protocol, addon_context->create_instance_done_cb_data);
  }

  ten_addon_context_destroy(addon_context);
  ten_app_thread_on_addon_create_protocol_done_ctx_destroy(ctx);
}

ten_app_thread_on_addon_create_addon_loader_done_ctx_t *
ten_app_thread_on_addon_create_addon_loader_done_ctx_create(void) {
  ten_app_thread_on_addon_create_addon_loader_done_ctx_t *self = TEN_MALLOC(
      sizeof(ten_app_thread_on_addon_create_addon_loader_done_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->addon_loader = NULL;
  self->addon_context = NULL;

  return self;
}

static void ten_app_thread_on_addon_create_addon_loader_done_ctx_destroy(
    ten_app_thread_on_addon_create_addon_loader_done_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}

void ten_app_thread_on_addon_create_addon_loader_done(void *self, void *arg) {
  ten_app_t *app = (ten_app_t *)self;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_app_thread_on_addon_create_addon_loader_done_ctx_t *ctx =
      (ten_app_thread_on_addon_create_addon_loader_done_ctx_t *)arg;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_addon_loader_t *addon_loader = ctx->addon_loader;
  ten_addon_context_t *addon_context = ctx->addon_context;

  TEN_ASSERT(addon_context, "Invalid argument.");

  if (addon_context->create_instance_done_cb) {
    addon_context->create_instance_done_cb(
        app->ten_env, addon_loader,
        addon_context->create_instance_done_cb_data);
  }

  ten_addon_context_destroy(addon_context);
  ten_app_thread_on_addon_create_addon_loader_done_ctx_destroy(ctx);
}

bool ten_app_on_ten_env_proxy_released(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  ten_app_t *app = self->attached_target.app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  if (!ten_list_is_empty(&self->ten_proxy_list)) {
    // There is still the presence of ten_env_proxy, so the closing process
    // cannot continue.
    TEN_LOGI(
        "[%s] Waiting for ten_env_proxy to be released, remaining %d "
        "ten_env_proxy(s).",
        ten_app_get_uri(app), ten_list_size(&self->ten_proxy_list));
    return true;
  }

  TEN_LOGI("Stop app's runloop.");

  ten_runloop_stop(app->loop);

  return true;
}
