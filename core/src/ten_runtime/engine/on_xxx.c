//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/engine/on_xxx.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_context/ten_env/on_xxx.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/extension_thread/on_xxx.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/sanitizer/thread_check.h"

void ten_engine_on_remove_extension_thread_from_engine(void *self_, void *arg) {
  ten_engine_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_extension_thread_t *extension_thread = arg;
  TEN_ASSERT(extension_thread, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: this function does not access this extension_thread, we
      // just check if the arg is an ten_extension_thread_t.
      ten_extension_thread_check_integrity(extension_thread, false),
      "Should not happen.");

  TEN_LOGD("[%s] Remove extension thread (%p) from engine.",
           ten_engine_get_id(self, true), extension_thread);

  ten_list_remove_ptr(&self->extension_context->extension_threads,
                      extension_thread);

  int rc = ten_runloop_post_task_tail(
      extension_thread->runloop, ten_extension_thread_on_removed_from_engine,
      extension_thread, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to extension thread's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_engine_on_extension_thread_closed_task(void *self_, void *arg) {
  ten_engine_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_extension_thread_t *extension_thread = arg;
  TEN_ASSERT(extension_thread, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: this function does not access this extension_thread, we
      // just check if the arg is an ten_extension_thread_t.
      ten_extension_thread_check_integrity(extension_thread, false),
      "Should not happen.");

  TEN_LOGD("[%s] Waiting for extension thread (%p) be reclaimed.",
           ten_engine_get_id(self, true), extension_thread);

  TEN_UNUSED int rc =
      ten_thread_join(ten_sanitizer_thread_check_get_belonging_thread(
                          &extension_thread->thread_check),
                      -1);
  TEN_ASSERT(!rc, "Should not happen.");

  TEN_LOGD("[%s] Extension thread (%p) is reclaimed.",
           ten_engine_get_id(self, true), extension_thread);

  // Extension thread is disappear, so we migrate the extension_group and
  // extension_thread to the engine thread now.
  ten_sanitizer_thread_check_inherit_from(&extension_thread->thread_check,
                                          &self->thread_check);

  // Extension thread is joined, so we can destroy it.
  ten_extension_thread_destroy(extension_thread);

  self->extension_context->extension_threads_cnt_of_closed++;

  ten_extension_context_on_close(self->extension_context);
}

void ten_engine_on_addon_create_extension_group_done(void *self_, void *arg) {
  ten_engine_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(self, true), "Should not happen.");

  ten_extension_context_on_addon_create_extension_group_done_ctx_t *ctx = arg;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_extension_group_t *extension_group = ctx->extension_group;
  TEN_ASSERT(extension_group &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The extension thread has not been created
                 // yet, so it is thread safe
                 ten_extension_group_check_integrity(extension_group, false),
             "Should not happen.");

  ten_extension_context_on_addon_create_extension_group_done(
      self->ten_env, extension_group, ctx->addon_context);

  ten_extension_context_on_addon_create_extension_group_done_ctx_destroy(ctx);
}

ten_engine_thread_on_addon_create_protocol_done_ctx_t *
ten_engine_thread_on_addon_create_protocol_done_ctx_create(void) {
  ten_engine_thread_on_addon_create_protocol_done_ctx_t *self =
      TEN_MALLOC(sizeof(ten_engine_thread_on_addon_create_protocol_done_ctx_t));

  self->protocol = NULL;
  self->addon_context = NULL;

  return self;
}

static void ten_engine_thread_on_addon_create_protocol_done_ctx_destroy(
    ten_engine_thread_on_addon_create_protocol_done_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}

void ten_engine_thread_on_addon_create_protocol_done(void *self, void *arg) {
  ten_engine_t *engine = self;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  ten_engine_thread_on_addon_create_protocol_done_ctx_t *ctx = arg;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_protocol_t *protocol = ctx->protocol;
  ten_addon_context_t *addon_context = ctx->addon_context;
  TEN_ASSERT(addon_context, "Should not happen.");

  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &protocol->thread_check);
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "Should not happen.");

  if (addon_context->create_instance_done_cb) {
    addon_context->create_instance_done_cb(
        engine->ten_env, protocol, addon_context->create_instance_done_cb_data);
  }

  ten_addon_context_destroy(addon_context);
  ten_engine_thread_on_addon_create_protocol_done_ctx_destroy(ctx);
}
