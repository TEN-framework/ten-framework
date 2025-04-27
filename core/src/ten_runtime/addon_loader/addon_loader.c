//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon_loader/addon_loader.h"

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/addon/common/common.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/on_xxx.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/macro/memory.h"

bool ten_addon_loader_singleton_store_check_integrity(
    ten_addon_loader_singleton_store_t *store, bool check_thread) {
  TEN_ASSERT(store, "Invalid argument.");

  if (ten_signature_get(&store->signature) !=
      (ten_signature_t)TEN_ADDON_LOADER_SINGLETON_STORE_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&store->thread_check);
  }

  return true;
}

void ten_addon_loader_singleton_store_init(
    ten_addon_loader_singleton_store_t *store) {
  TEN_ASSERT(store, "Can not init empty addon store.");
  ten_signature_set(&store->signature,
                    TEN_ADDON_LOADER_SINGLETON_STORE_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&store->thread_check);

  ten_list_init(&store->store);
}

void ten_addon_loader_singleton_store_deinit(
    ten_addon_loader_singleton_store_t *store) {
  TEN_ASSERT(store, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(store, false),
             "Invalid argument.");

  TEN_ASSERT(ten_list_is_empty(&store->store), "Should not happen.");

  ten_sanitizer_thread_check_deinit(&store->thread_check);
}

bool ten_addon_loader_check_integrity(ten_addon_loader_t *self,
                                      bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ADDON_LOADER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

ten_env_t *ten_addon_loader_get_ten_env(ten_addon_loader_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(self, true), "Invalid argument.");

  return self->ten_env;
}

ten_addon_loader_t *ten_addon_loader_create(
    ten_addon_loader_on_init_func_t on_init,
    ten_addon_loader_on_deinit_func_t on_deinit,
    ten_addon_loader_on_load_addon_func_t on_load_addon) {
  ten_addon_loader_t *self =
      (ten_addon_loader_t *)TEN_MALLOC(sizeof(ten_addon_loader_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_ADDON_LOADER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->addon_host = NULL;

  self->on_init = on_init;
  self->on_deinit = on_deinit;
  self->on_load_addon = on_load_addon;

  self->on_init_done = NULL;
  self->on_init_done_data = NULL;

  self->ten_env = ten_env_create_for_addon_loader(self);

  return self;
}

void ten_addon_loader_destroy(ten_addon_loader_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(self->ten_env, "Should not happen.");

  ten_signature_set(&self->signature, 0);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  ten_env_destroy(self->ten_env);

  TEN_FREE(self);
}

static void ten_addon_loader_init(ten_addon_loader_t *self,
                                  ten_addon_loader_on_init_done_func_t cb,
                                  void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(self, true), "Invalid argument.");

  self->on_init_done = cb;
  self->on_init_done_data = cb_data;

  if (self->on_init) {
    self->on_init(self, self->ten_env);
  } else {
    ten_env_on_init_done(self->ten_env, NULL);
  }
}

static void ten_addon_loader_deinit(ten_addon_loader_t *self,
                                    ten_addon_loader_on_deinit_done_func_t cb,
                                    void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(self, true), "Invalid argument.");

  self->on_deinit_done = cb;
  self->on_deinit_done_data = cb_data;

  if (self->on_deinit) {
    self->on_deinit(self, self->ten_env);
  } else {
    ten_env_on_deinit_done(self->ten_env, NULL);
  }
}

void ten_addon_loader_load_addon(ten_addon_loader_t *self,
                                 TEN_ADDON_TYPE addon_type,
                                 const char *addon_name,
                                 ten_addon_loader_on_load_addon_done_func_t cb,
                                 void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(self, true), "Invalid argument.");

  ten_addon_loader_load_addon_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_addon_loader_load_addon_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->cb = cb;
  ctx->cb_data = cb_data;

  TEN_ASSERT(self->on_load_addon, "Should not happen.");
  self->on_load_addon(self, self->ten_env, addon_type, addon_name, ctx);
}

bool ten_addon_loader_on_load_addon_done(ten_env_t *ten_env, void *context) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_ADDON_LOADER,
             "Should not happen.");

  ten_addon_loader_load_addon_ctx_t *ctx =
      (ten_addon_loader_load_addon_ctx_t *)context;
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->cb) {
    ctx->cb(ten_env, ctx->cb_data);
  }

  TEN_FREE(ctx);

  return true;
}

static void ten_app_thread_on_addon_loader_init_done(void *app_,
                                                     void *cb_data) {
  ten_app_t *app = (ten_app_t *)app_;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_addon_loader_singleton_store_t *store =
      &app->addon_loader_singleton_store;
  TEN_ASSERT(store, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(store, true),
             "Should not happen.");

  ten_app_on_addon_loader_init_done_ctx_t *ctx =
      (ten_app_on_addon_loader_init_done_ctx_t *)cb_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_addon_loader_on_all_singleton_instances_created_ctx_t
      *on_all_created_ctx = ctx->cb_data;
  TEN_ASSERT(on_all_created_ctx, "Invalid argument.");

  size_t desired_count = on_all_created_ctx->desired_count;

  ten_list_push_ptr_back(&store->store, ctx->addon_loader, NULL);

  TEN_FREE(ctx);

  if (ten_list_size(&store->store) == desired_count) {
    // Call the callback to notify all addon_loader instances have been created.
    on_all_created_ctx->cb(on_all_created_ctx->ten_env,
                           on_all_created_ctx->cb_data);

    TEN_FREE(on_all_created_ctx);
  }
}

static void ten_app_thread_on_addon_loader_deinit_done(void *app_,
                                                       void *cb_data) {
  ten_app_t *app = (ten_app_t *)app_;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_addon_loader_singleton_store_t *store =
      &app->addon_loader_singleton_store;
  TEN_ASSERT(store, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(store, true),
             "Should not happen.");

  ten_app_on_addon_loader_deinit_done_ctx_t *ctx =
      (ten_app_on_addon_loader_deinit_done_ctx_t *)cb_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t
      *on_all_destroyed_ctx = ctx->cb_data;
  TEN_ASSERT(on_all_destroyed_ctx, "Invalid argument.");

  ten_addon_loader_t *addon_loader = ctx->addon_loader;
  TEN_ASSERT(addon_loader, "Invalid argument.");
  TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
             "Invalid argument.");

  ten_addon_host_t *addon_host = addon_loader->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");

  addon_host->addon->on_destroy_instance(addon_host->addon, addon_host->ten_env,
                                         addon_loader, NULL);

  ten_list_remove_ptr(&store->store, addon_loader);

  if (ten_list_size(&store->store) == 0) {
    if (on_all_destroyed_ctx->cb) {
      on_all_destroyed_ctx->cb(on_all_destroyed_ctx->ten_env,
                               on_all_destroyed_ctx->cb_data);
    }

    TEN_FREE(on_all_destroyed_ctx);
  }

  TEN_FREE(ctx);
}

static void ten_addon_loader_init_done(ten_env_t *ten_env, void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(cb_data, "Invalid argument.");

  ten_addon_loader_t *addon_loader = ten_env_get_attached_addon_loader(ten_env);
  TEN_ASSERT(addon_loader, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
             "Should not happen.");

  ten_addon_loader_on_all_singleton_instances_created_ctx_t *ctx =
      (ten_addon_loader_on_all_singleton_instances_created_ctx_t *)cb_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_env_t *caller_ten_env = ctx->ten_env;
  TEN_ASSERT(caller_ten_env, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(caller_ten_env, false),
             "Invalid argument.");

  TEN_ASSERT(caller_ten_env->attach_to == TEN_ENV_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(caller_ten_env);
  TEN_ASSERT(app, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid argument.");

  ten_app_on_addon_loader_init_done_ctx_t *on_addon_loader_init_done_ctx =
      TEN_MALLOC(sizeof(ten_app_on_addon_loader_init_done_ctx_t));
  TEN_ASSERT(on_addon_loader_init_done_ctx, "Should not happen.");

  on_addon_loader_init_done_ctx->addon_loader = addon_loader;
  on_addon_loader_init_done_ctx->cb_data = ctx;

  int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(app),
                                      ten_app_thread_on_addon_loader_init_done,
                                      app, on_addon_loader_init_done_ctx);
  if (rc) {
    TEN_LOGE("Failed to post task to app runloop.");
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void create_addon_loader_done(ten_env_t *ten_env,
                                     ten_addon_loader_t *addon_loader,
                                     void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(
      addon_loader && ten_addon_loader_check_integrity(addon_loader, true),
      "Invalid argument.");
  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_APP,
             "Invalid argument.");

  ten_app_t *app = ten_env_get_attached_target(ten_env);
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  // Call `on_init` of the addon_loader to initialize the addon_loader.
  ten_addon_loader_init(addon_loader, ten_addon_loader_init_done, cb_data);
}

static ten_addon_loader_on_all_singleton_instances_created_ctx_t *
ten_addon_loader_on_all_singleton_instances_created_ctx_create(
    ten_env_t *ten_env, size_t desired_count,
    ten_addon_loader_on_all_singleton_instances_created_cb_t cb,
    void *cb_data) {
  ten_addon_loader_on_all_singleton_instances_created_ctx_t *ctx = TEN_MALLOC(
      sizeof(ten_addon_loader_on_all_singleton_instances_created_ctx_t));
  TEN_ASSERT(ctx, "Should not happen.");

  ctx->ten_env = ten_env;
  ctx->desired_count = desired_count;
  ctx->cb = cb;
  ctx->cb_data = cb_data;

  return ctx;
}

static ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t *
ten_addon_loader_on_all_singleton_instances_destroyed_ctx_create(
    ten_env_t *ten_env,
    ten_addon_loader_on_all_singleton_instances_destroyed_cb_t cb,
    void *cb_data) {
  ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t *ctx = TEN_MALLOC(
      sizeof(ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->ten_env = ten_env;
  ctx->cb = cb;
  ctx->cb_data = cb_data;

  return ctx;
}

void ten_addon_loader_addons_create_singleton_instance(
    ten_env_t *ten_env,
    ten_addon_loader_on_all_singleton_instances_created_cb_t cb,
    void *cb_data) {
  bool need_to_wait_all_addon_loaders_created = true;

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_store_t *addon_loader_store = &app->addon_loader_store;
  TEN_ASSERT(addon_loader_store, "Should not happen.");
  TEN_ASSERT(ten_addon_store_check_integrity(addon_loader_store, true),
             "Should not happen.");

  ten_addon_loader_singleton_store_t *addon_loader_singleton_store =
      &app->addon_loader_singleton_store;
  TEN_ASSERT(addon_loader_singleton_store, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(
                 addon_loader_singleton_store, true),
             "Should not happen.");

  size_t desired_count = ten_list_size(&addon_loader_store->store);
  if (!desired_count ||
      ten_list_size(&addon_loader_singleton_store->store) == desired_count) {
    need_to_wait_all_addon_loaders_created = false;
    goto done;
  }

  ten_addon_loader_on_all_singleton_instances_created_ctx_t *ctx =
      ten_addon_loader_on_all_singleton_instances_created_ctx_create(
          ten_env, desired_count, cb, cb_data);
  TEN_ASSERT(ctx, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_list_foreach (&addon_loader_store->store, iter) {
    ten_addon_host_t *loader_addon_host = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(loader_addon_host, "Should not happen.");

    bool res = ten_addon_create_addon_loader(
        ten_env, ten_string_get_raw_str(&loader_addon_host->name),
        ten_string_get_raw_str(&loader_addon_host->name),
        (ten_env_addon_create_instance_done_cb_t)create_addon_loader_done, ctx,
        &err);

    if (!res) {
      TEN_LOGE("Failed to create addon_loader instance %s, %s.",
               ten_string_get_raw_str(&loader_addon_host->name),
               ten_error_message(&err));
#if defined(_DEBUG)
      TEN_ASSERT(0, "Should not happen.");
#endif
    }
  }

  ten_error_deinit(&err);

done:
  if (!need_to_wait_all_addon_loaders_created && cb) {
    cb(ten_env, cb_data);
  }
}

static void ten_addon_loader_deinit_done(ten_env_t *ten_env, void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(cb_data, "Invalid argument.");

  ten_addon_loader_t *addon_loader = ten_env_get_attached_addon_loader(ten_env);
  TEN_ASSERT(addon_loader, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
             "Should not happen.");

  ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t *ctx =
      (ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t *)cb_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ten_env_t *caller_ten_env = ctx->ten_env;
  TEN_ASSERT(caller_ten_env, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(caller_ten_env, false),
             "Invalid argument.");

  TEN_ASSERT(caller_ten_env->attach_to == TEN_ENV_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(caller_ten_env);
  TEN_ASSERT(app, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_app_check_integrity(app, false), "Invalid argument.");

  ten_app_on_addon_loader_deinit_done_ctx_t *on_addon_loader_deinit_done_ctx =
      TEN_MALLOC(sizeof(ten_app_on_addon_loader_deinit_done_ctx_t));
  TEN_ASSERT(on_addon_loader_deinit_done_ctx, "Should not happen.");

  on_addon_loader_deinit_done_ctx->addon_loader = addon_loader;
  on_addon_loader_deinit_done_ctx->cb_data = ctx;

  int rc =
      ten_runloop_post_task_tail(ten_app_get_attached_runloop(app),
                                 ten_app_thread_on_addon_loader_deinit_done,
                                 app, on_addon_loader_deinit_done_ctx);
  if (rc) {
    TEN_LOGE("Failed to post task to app runloop.");
    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_addon_loader_destroy_all_singleton_instances(
    ten_env_t *ten_env,
    ten_addon_loader_on_all_singleton_instances_destroyed_cb_t cb,
    void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_loader_singleton_store_t *addon_loader_singleton_store =
      &app->addon_loader_singleton_store;
  TEN_ASSERT(addon_loader_singleton_store, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(
                 addon_loader_singleton_store, true),
             "Should not happen.");

  bool need_to_wait_all_addon_loaders_destroyed = true;

  if (ten_list_size(&addon_loader_singleton_store->store) == 0) {
    need_to_wait_all_addon_loaders_destroyed = false;
    goto done;
  }

  ten_addon_loader_on_all_singleton_instances_destroyed_ctx_t *ctx =
      ten_addon_loader_on_all_singleton_instances_destroyed_ctx_create(
          ten_env, cb, cb_data);
  TEN_ASSERT(ctx, "Should not happen.");

  ten_list_foreach (&addon_loader_singleton_store->store, iter) {
    ten_addon_loader_t *addon_loader = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon_loader, "Should not happen.");
    TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
               "Should not happen.");

    ten_addon_host_t *addon_host = addon_loader->addon_host;
    TEN_ASSERT(addon_host, "Should not happen.");
    TEN_ASSERT(ten_addon_host_check_integrity(addon_host),
               "Should not happen.");

    ten_addon_loader_deinit(addon_loader, ten_addon_loader_deinit_done, ctx);
  }

done:
  if (!need_to_wait_all_addon_loaders_destroyed && cb) {
    cb(ten_env, cb_data);
  }
}