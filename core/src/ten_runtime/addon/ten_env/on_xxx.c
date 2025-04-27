//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/ten_env/on_xxx.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/common/store.h"
#include "include_internal/ten_runtime/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/app/on_xxx.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/on_xxx.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/ten_env/on_xxx.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/on_xxx.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

void ten_addon_on_init_done(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_handle_manifest_info_when_on_configure_done(
      &addon_host->manifest_info, NULL, &addon_host->manifest, &err);
  if (!rc) {
    TEN_LOGW("Failed to load addon manifest data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  rc = ten_handle_property_info_when_on_configure_done(
      &addon_host->property_info, NULL, &addon_host->property, &err);
  if (!rc) {
    TEN_LOGW("Failed to load addon property data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  ten_value_t *manifest_name_value =
      ten_value_object_peek(&addon_host->manifest, TEN_STR_NAME);

  const char *manifest_name = NULL;
  if (manifest_name_value) {
    manifest_name = ten_value_peek_raw_str(manifest_name_value, &err);
  }

  ten_error_deinit(&err);

  if (manifest_name) {
    TEN_ASSERT(manifest_name, "Should not happen.");

    if (ten_string_len(&addon_host->name) &&
        !ten_string_is_equal_c_str(&addon_host->name, manifest_name)) {
      TEN_LOGW(
          "The registered extension name (%s) is not equal to the name (%s) in "
          "the manifest.",
          ten_string_get_raw_str(&addon_host->name), manifest_name);

      // Get 'name' from manifest, and check the consistency between the name
      // specified in the argument, and the name specified in the manifest.
      //
      // The name in the manifest could be checked by the TEN store to ensure
      // the uniqueness of the name.
      TEN_ASSERT(0, "Should not happen.");
    }

    // If an extension defines an extension name in its manifest file, TEN
    // runtime would use that name instead of the name specified in the codes to
    // register it to the extension store.
    if (strlen(manifest_name)) {
      ten_string_set_from_c_str(&addon_host->name, manifest_name);
    }
  }
}

bool ten_addon_on_deinit_done(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  ten_env_close(self);

  if (addon_host->addon->on_destroy) {
    addon_host->addon->on_destroy(addon_host->addon);
  }

  ten_addon_host_destroy(addon_host);

  return true;
}

static void ten_extension_addon_on_create_instance_done(ten_env_t *self,
                                                        void *instance,
                                                        void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  ten_addon_context_t *addon_context = (ten_addon_context_t *)context;
  TEN_ASSERT(addon_context, "Invalid argument.");

  TEN_ASSERT(addon_context->create_instance_done_cb, "Should not happen.");

  switch (addon_context->flow) {
  case TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_CREATE_EXTENSION: {
    ten_extension_t *extension = instance;
    if (extension) {
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: Maybe in the thread other than the extension thread
      // (ex: JS main thread), and all the function calls in this case are
      // thread safe.
      TEN_ASSERT(ten_extension_check_integrity(extension, false),
                 "Should not happen.");

      ten_extension_set_addon(extension, addon_host);
    }

    ten_extension_thread_t *extension_thread =
        addon_context->flow_target.extension_thread;
    TEN_ASSERT(extension_thread, "Should not happen.");
    TEN_ASSERT(
        // TEN_NOLINTNEXTLINE(thread-check)
        // thread-check: Maybe in the thread other than the extension
        // thread (ex: JS main thread), and all the function calls in
        // this case are thread safe.
        ten_extension_thread_check_integrity(extension_thread, false),
        "Should not happen.");

    ten_extension_thread_on_addon_create_extension_done_ctx_t *ctx =
        ten_extension_thread_on_addon_create_extension_done_ctx_create();

    ctx->extension = extension;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(
        ten_extension_thread_get_attached_runloop(extension_thread),
        ten_extension_thread_on_addon_create_extension_done, extension_thread,
        ctx);
    TEN_ASSERT(!rc, "Should not happen.");

    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

static void ten_extension_group_addon_on_create_instance_done(ten_env_t *self,
                                                              void *instance,
                                                              void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  ten_addon_context_t *addon_context = (ten_addon_context_t *)context;
  TEN_ASSERT(addon_context, "Invalid argument.");

  TEN_ASSERT(addon_context->create_instance_done_cb, "Should not happen.");

  switch (addon_context->flow) {
  case TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group = instance;
    TEN_ASSERT(extension_group &&
                   // TEN_NOLINTNEXTLINE(thread-check)
                   // thread-check: The extension thread has not been created
                   // yet, so it is thread safe at this time.
                   ten_extension_group_check_integrity(extension_group, false),
               "Invalid argument.");

    ten_extension_group_set_addon(extension_group, addon_host);

    ten_engine_t *engine = addon_context->flow_target.engine;
    TEN_ASSERT(engine, "Invalid argument.");
    TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

    ten_extension_context_on_addon_create_extension_group_done_ctx_t *ctx =
        ten_extension_context_on_addon_create_extension_group_done_ctx_create();

    ctx->extension_group = extension_group;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(
        ten_engine_get_attached_runloop(engine),
        ten_engine_on_addon_create_extension_group_done, engine, ctx);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

static void ten_protocol_addon_on_create_instance_done(ten_env_t *self,
                                                       void *instance,
                                                       void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in the app thread.
  // TODO(xilin): change false to true.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");
  TEN_ASSERT(addon_host->type == TEN_ADDON_TYPE_PROTOCOL, "Should not happen.");

  ten_protocol_t *protocol = instance;
  TEN_ASSERT(protocol && ten_protocol_check_integrity(protocol, false),
             "Should not happen.");

  if (!protocol->addon_host) {
    ten_protocol_set_addon(protocol, addon_host);
  }

  ten_addon_context_t *addon_context = (ten_addon_context_t *)context;
  if (!addon_context) {
    return;
  }

  TEN_ASSERT(addon_context->create_instance_done_cb, "Should not happen.");

  switch (addon_context->flow) {
  case TEN_ADDON_CONTEXT_FLOW_ENGINE_CREATE_PROTOCOL: {
    ten_engine_t *engine = addon_context->flow_target.engine;
    TEN_ASSERT(engine, "Should not happen.");
    // TEN_NOLINTNEXTLINE(thread-check)
    // thread-check: We are currently in the protocol creation process, during
    // which the engine cannot be closed or destroyed (because protocol creation
    // depends on the engine, so the engine's logic ensures that while a
    // protocol is being created, the engine cannot be closed or destroyed).
    // Therefore, accessing the engine instance at this point is safe.
    TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

    ten_engine_thread_on_addon_create_protocol_done_ctx_t *ctx =
        ten_engine_thread_on_addon_create_protocol_done_ctx_create();

    ctx->protocol = protocol;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(
        ten_engine_get_attached_runloop(engine),
        ten_engine_thread_on_addon_create_protocol_done, engine, ctx);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  case TEN_ADDON_CONTEXT_FLOW_APP_CREATE_PROTOCOL: {
    ten_app_t *app = addon_context->flow_target.app;
    TEN_ASSERT(
        app && ten_app_check_integrity(
                   app,
                   // TEN_NOLINTNEXTLINE(thread-check)
                   // thread-check: We are probably on another app thread
                   // (ex: fake_app thread), and the target app is not closed
                   // at this time so it is safe to access the app instance.
                   false),
        "Should not happen.");

    ten_app_thread_on_addon_create_protocol_done_ctx_t *ctx =
        ten_app_thread_on_addon_create_protocol_done_ctx_create();

    ctx->protocol = instance;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(
        ten_app_get_attached_runloop(app),
        ten_app_thread_on_addon_create_protocol_done, app, ctx);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

static void ten_addon_loader_addon_on_create_instance_done(ten_env_t *self,
                                                           void *instance,
                                                           void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");
  TEN_ASSERT(addon_host->type == TEN_ADDON_TYPE_ADDON_LOADER,
             "Should not happen.");

  ten_addon_loader_t *addon_loader = instance;
  TEN_ASSERT(addon_loader, "Should not happen.");

  addon_loader->addon_host = addon_host;

  ten_addon_context_t *addon_context = (ten_addon_context_t *)context;
  if (!addon_context) {
    return;
  }

  TEN_ASSERT(addon_context->create_instance_done_cb, "Should not happen.");

  switch (addon_context->flow) {
  case TEN_ADDON_CONTEXT_FLOW_APP_CREATE_ADDON_LOADER: {
    ten_app_t *app = addon_context->flow_target.app;
    TEN_ASSERT(app && ten_app_check_integrity(app, true), "Should not happen.");

    ten_app_thread_on_addon_create_addon_loader_done_ctx_t *ctx =
        ten_app_thread_on_addon_create_addon_loader_done_ctx_create();

    ctx->addon_loader = instance;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(
        ten_app_get_attached_runloop(app),
        ten_app_thread_on_addon_create_addon_loader_done, app, ctx);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

void ten_addon_on_create_instance_done(ten_env_t *self, void *instance,
                                       void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  switch (addon_host->type) {
  case TEN_ADDON_TYPE_EXTENSION:
    ten_extension_addon_on_create_instance_done(self, instance, context);
    break;
  case TEN_ADDON_TYPE_EXTENSION_GROUP:
    ten_extension_group_addon_on_create_instance_done(self, instance, context);
    break;
  case TEN_ADDON_TYPE_PROTOCOL:
    ten_protocol_addon_on_create_instance_done(self, instance, context);
    break;
  case TEN_ADDON_TYPE_ADDON_LOADER:
    ten_addon_loader_addon_on_create_instance_done(self, instance, context);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}

void ten_addon_on_destroy_instance_done(ten_env_t *self, void *context) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
  TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
             "Should not happen.");

  ten_addon_context_t *addon_context = (ten_addon_context_t *)context;
  if (!addon_context) {
    // If the addon_context is NULL, it means that the result of destroy does
    // not need to be handled, so we can return directly.

    // TODO(xilin): For the destroy of the protocol, no `addon_context`
    // parameter is passed in, which means there’s also no `caller rte`
    // parameter. Since there’s no `caller rte` parameter, there’s no action to
    // enqueue a task to the thread where the `caller rte` is located. This is
    // because, on one hand, there’s currently no place that requires waiting
    // for protocol destroy to complete, and on the other hand, the app/engine
    // thread may have already exited at this point. Therefore, the
    // `on_destroy_instance_done` of the protocol is actually not called in the
    // current implementation.

    // TODO(xilin): It may be necessary to adjust the destruction process so
    // that the destroy of the protocol executes on the correct thread.
    return;
  }

  TEN_ASSERT(addon_context, "Invalid argument.");
  TEN_ASSERT(addon_context->destroy_instance_done_cb, "Should not happen.");

  switch (addon_context->flow) {
  case TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION_GROUP: {
    ten_extension_thread_t *extension_thread =
        addon_context->flow_target.extension_thread;
    TEN_ASSERT(extension_thread, "Should not happen.");
    TEN_ASSERT(
        // TEN_NOLINTNEXTLINE(thread-check)
        // thread-check: Maybe in the thread other than the
        // engine thread (ex: JS main thread), and all the
        // function calls in this case are thread safe.
        ten_extension_thread_check_integrity(extension_thread, false),
        "Should not happen.");

    int rc = ten_runloop_post_task_tail(
        ten_extension_thread_get_attached_runloop(extension_thread),
        ten_extension_thread_on_addon_destroy_extension_group_done,
        extension_thread, addon_context);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  case TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION: {
    ten_extension_thread_t *extension_thread =
        addon_context->flow_target.extension_thread;
    TEN_ASSERT(extension_thread, "Should not happen.");
    TEN_ASSERT(
        // TEN_NOLINTNEXTLINE(thread-check)
        // thread-check: Maybe in the thread other than the engine
        // thread (ex: JS main thread), and all the function calls in
        // this case are thread safe.
        ten_extension_thread_check_integrity(extension_thread, false),
        "Should not happen.");

    int rc = ten_runloop_post_task_tail(
        ten_extension_thread_get_attached_runloop(extension_thread),
        ten_extension_thread_on_addon_destroy_extension_done, extension_thread,
        addon_context);
    TEN_ASSERT(!rc, "Should not happen.");
    break;
  }

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }
}
