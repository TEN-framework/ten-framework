//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_thread/on_xxx.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/on_xxx.h"
#include "include_internal/ten_runtime/extension/close.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/metadata.h"
#include "include_internal/ten_runtime/extension/msg_handling.h"
#include "include_internal/ten_runtime/extension/path_timer.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/base_dir.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/metadata.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/extension_store/extension_store.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/msg_interface/common.h"
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/path/path_table.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/extension/extension.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"

void ten_extension_inherit_thread_ownership(
    ten_extension_t *self, ten_extension_thread_t *extension_thread) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The correct threading ownership will be setup
                 // soon, so we can _not_ check thread safety here.
                 ten_extension_check_integrity(self, false),
             "Should not happen.");

  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  // Move the ownership of the extension relevant resources to the current
  // thread.
  ten_sanitizer_thread_check_inherit_from(&self->thread_check,
                                          &extension_thread->thread_check);
  ten_sanitizer_thread_check_inherit_from(&self->ten_env->thread_check,
                                          &extension_thread->thread_check);
}

void ten_extension_thread_on_extension_group_on_init_done(
    void *self_, TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  // The extension system is about to be shut down, so do not proceed with
  // initialization anymore.
  if (self->is_close_triggered) {
    return;
  }

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_handle_manifest_info_when_on_configure_done(
      &extension_group->manifest_info,
      ten_extension_group_get_base_dir(extension_group),
      &extension_group->manifest, &err);
  if (!rc) {
    TEN_LOGW("Failed to load extension group manifest data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  rc = ten_handle_property_info_when_on_configure_done(
      &extension_group->property_info,
      ten_extension_group_get_base_dir(extension_group),
      &extension_group->property, &err);
  if (!rc) {
    TEN_LOGW("Failed to load extension group property data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  ten_error_deinit(&err);

  ten_extension_group_create_extensions(self->extension_group);
}

void ten_extension_thread_stop_life_cycle_of_all_extensions(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid argument.");

  ten_extension_thread_set_state(self,
                                 TEN_EXTENSION_THREAD_STATE_PREPARE_TO_CLOSE);

  if (ten_list_is_empty(&self->extensions)) {
    // This extension group does not contain any extensions, so it can directly
    // proceed to the deinitialization phase of the extension group.
    ten_extension_group_on_deinit(self->extension_group);
  } else {
    // Loop for all the containing extensions, and call their on_stop().
    ten_list_foreach (&self->extensions, iter) {
      ten_extension_t *extension = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(ten_extension_check_integrity(extension, true),
                 "Should not happen.");

      TEN_EXTENSION_STATE state = extension->state;
      // At this point, the state of the extension should __not__ be in
      // TEN_EXTENSION_STATE_ON_STOP or thereafter.
      TEN_ASSERT(state < TEN_EXTENSION_STATE_ON_STOP,
                 "The extension %s is in the TEN_EXTENSION_STATE_ON_STOP or "
                 "afterward state, this should not happen.",
                 ten_string_get_raw_str(&extension->name));

      // The `on_xxx` callbacks (such as `on_configure`, `on_init`, `on_start`)
      // should not directly transition to `on_stop` just because the app or
      // graph is about to terminate. Instead, the transition to `on_stop` must
      // wait until `on_xxx_done` is completed. This is because, before
      // `on_xxx_done` is completed, developers may still actively use the TEN
      // API (i.e., invoke APIs from `ten_env`). If the system transitions to
      // `on_stop` (and subsequently `on_deinit_done`) without waiting for
      // `on_xxx_done`, it would require almost every use of the `ten_env` API
      // to check whether the TEN environment has already terminated. This would
      // result in a poor development experience. Allowing the formal closing
      // flow (i.e., entering `on_stop`) only after `on_xxx_done` seems to avoid
      // any such issues. Therefore, this process and logic have been adopted
      // for now.
      //
      // For extensions in the TEN_EXTENSION_STATE_ON_XXX state, when their
      // corresponding on_xxx_done() is called, it will check the
      // extension_thread_state. If it is
      // TEN_EXTENSION_THREAD_STATE_PREPARE_TO_CLOSE, the on_stop() method will
      // be called immediately afterward. That is to say, for extensions in the
      // TEN_EXTENSION_STATE_ON_XXX_DONE state, their on_stop() method can be
      // called directly.

      if (state == TEN_EXTENSION_STATE_ON_CONFIGURE_DONE ||
          state == TEN_EXTENSION_STATE_ON_INIT_DONE ||
          state == TEN_EXTENSION_STATE_ON_START_DONE) {
        ten_extension_on_stop(extension);
      }

      // If the extension is in the TEN_EXTENSION_STATE_ON_XXX state, we need
      // to wait until the corresponding on_xxx_done() is called, and then
      // call the on_stop() method.
    }
  }
}

static void ten_extension_context_on_extension_group_destroyed(
    TEN_UNUSED ten_env_t *ten_env, void *cb_data) {
  ten_extension_thread_t *extension_thread = (ten_extension_thread_t *)cb_data;
  TEN_ASSERT(extension_thread, "Should not happen.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  extension_thread->extension_group = NULL;

  // All resources of the extension thread are cleaned up, so we can stop the
  // runloop.
  ten_runloop_stop(extension_thread->runloop);
}

void ten_extension_thread_on_removed_from_engine(void *self_,
                                                 TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  // Start the destruction of the extension group.
  ten_addon_destroy_extension_group(
      extension_group->ten_env, extension_group,
      ten_extension_context_on_extension_group_destroyed, self);
}

static void ten_extension_thread_notify_engine_to_remove_from_engine(
    ten_extension_thread_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_engine_t *engine = self->extension_context->engine;
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: In the closing flow, the closing of the engine is always
  // after the closing of the extension thread, so its thread safe to access the
  // runloop of the engine here.
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_runloop_t *engine_loop = ten_engine_get_attached_runloop(engine);
  TEN_ASSERT(engine_loop, "Should not happen.");

  int rc = ten_runloop_post_task_tail(
      engine_loop, ten_engine_on_remove_extension_thread_from_engine, engine,
      self);
  if (rc) {
    TEN_LOGW("Failed to post task to engine's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_extension_thread_on_extension_group_on_deinit_done(
    void *self_, TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_thread_notify_engine_to_remove_from_engine(self);
}

void ten_extension_thread_on_all_extensions_deleted(void *self_,
                                                    TEN_UNUSED void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_list_clear(&self->extensions);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_extension_group_on_deinit(extension_group);
}

void ten_extension_thread_on_addon_create_extension_done(void *self_,
                                                         void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_extension_thread_on_addon_create_extension_done_ctx_t *ctx = arg;
  TEN_ASSERT(arg, "Should not happen.");

  ten_extension_t *extension = ctx->extension;
  if (extension) {
    ten_extension_inherit_thread_ownership(extension, self);
    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Should not happen.");
  }

  ten_extension_group_on_addon_create_extension_done(
      extension_group->ten_env, extension, ctx->addon_context);

  ten_extension_thread_on_addon_create_extension_done_ctx_destroy(ctx);
}

void ten_extension_thread_on_addon_destroy_extension_done(void *self_,
                                                          void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_extension_group_t *extension_group = self->extension_group;
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_addon_context_t *addon_context = arg;
  TEN_ASSERT(addon_context, "Should not happen.");

  ten_env_t *extension_group_ten = extension_group->ten_env;
  TEN_ASSERT(
      extension_group_ten && ten_env_check_integrity(extension_group_ten, true),
      "Should not happen.");

  // This happens on the extension thread, so it's thread safe.

  ten_extension_group_on_addon_destroy_extension_done(extension_group_ten,
                                                      addon_context);
}

void ten_extension_thread_create_extension_instance(void *self_, void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_addon_on_create_extension_instance_ctx_t *addon_instance_info = arg;
  TEN_ASSERT(addon_instance_info, "Should not happen.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  TEN_ASSERT(addon_context, "Failed to allocate memory.");

  ten_addon_context_set_creation_info(
      addon_context, TEN_ADDON_TYPE_EXTENSION,
      ten_string_get_raw_str(&addon_instance_info->addon_name),
      ten_string_get_raw_str(&addon_instance_info->instance_name));

  addon_context->flow =
      TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_CREATE_EXTENSION;
  addon_context->flow_target.extension_thread = self;
  addon_context->create_instance_done_cb = addon_instance_info->cb;
  addon_context->create_instance_done_cb_data = addon_instance_info->cb_data;

  bool rc = ten_addon_create_instance_async(self->extension_group->ten_env,
                                            addon_context);

  if (!rc) {
    ten_addon_context_destroy(addon_context);
  }

  ten_addon_on_create_extension_instance_ctx_destroy(addon_instance_info);
}

void ten_extension_thread_destroy_addon_instance(void *self_, void *arg) {
  ten_extension_thread_t *self = (ten_extension_thread_t *)self_;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Invalid use of extension_thread %p.", self);

  ten_addon_host_on_destroy_instance_ctx_t *destroy_instance_info = arg;
  TEN_ASSERT(destroy_instance_info, "Should not happen.");

  ten_addon_context_t *addon_context = ten_addon_context_create();
  addon_context->flow =
      TEN_ADDON_CONTEXT_FLOW_EXTENSION_THREAD_DESTROY_EXTENSION;
  addon_context->flow_target.extension_thread = self;
  addon_context->destroy_instance_done_cb = destroy_instance_info->cb;
  addon_context->destroy_instance_done_cb_data = destroy_instance_info->cb_data;

  ten_addon_host_destroy_instance_async(destroy_instance_info->addon_host,
                                        destroy_instance_info->instance,
                                        addon_context);

  ten_addon_host_on_destroy_instance_ctx_destroy(destroy_instance_info);
}

ten_extension_thread_on_addon_create_extension_done_ctx_t *
ten_extension_thread_on_addon_create_extension_done_ctx_create(void) {
  ten_extension_thread_on_addon_create_extension_done_ctx_t *self = TEN_MALLOC(
      sizeof(ten_extension_thread_on_addon_create_extension_done_ctx_t));

  self->addon_context = NULL;
  self->extension = NULL;

  return self;
}

void ten_extension_thread_on_addon_create_extension_done_ctx_destroy(
    ten_extension_thread_on_addon_create_extension_done_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}

void ten_extension_thread_on_addon_destroy_extension_group_done(void *self_,
                                                                void *arg) {
  ten_extension_thread_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_thread_check_integrity(self, true),
             "Should not happen.");

  ten_addon_context_t *addon_context = arg;
  TEN_ASSERT(addon_context, "Should not happen.");

  // This happens on the extension thread, so it's thread safe.

  if (addon_context->destroy_instance_done_cb) {
    addon_context->destroy_instance_done_cb(
        NULL, addon_context->destroy_instance_done_cb_data);
  }

  ten_addon_context_destroy(addon_context);
}
