//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_context/ten_env/on_xxx.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

void ten_extension_context_on_addon_create_extension_group_done(
    ten_env_t *self, void *instance, ten_addon_context_t *addon_context) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ENGINE, "Should not happen.");

  TEN_UNUSED ten_engine_t *engine = ten_env_get_attached_engine(self);
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, true), "Should not happen.");

  TEN_UNUSED ten_extension_group_t *extension_group = instance;
  TEN_ASSERT(extension_group &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The extension thread has not been created yet,
                 // so it is thread safe.
                 ten_extension_group_check_integrity(extension_group, false),
             "Should not happen.");

  ten_env_t *extension_group_ten = extension_group->ten_env;
  TEN_ASSERT(extension_group_ten, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The extension thread has not been created yet, so it is
  // thread safe.
  TEN_ASSERT(ten_env_check_integrity(extension_group_ten, false),
             "Invalid use of ten_env %p.", extension_group_ten);

  // This happens on the engine thread, so it's thread safe.

  if (addon_context->create_instance_done_cb) {
    addon_context->create_instance_done_cb(
        self, instance, addon_context->create_instance_done_cb_data);
  }

  if (addon_context) {
    ten_addon_context_destroy(addon_context);
  }
}

ten_extension_context_on_addon_create_extension_group_done_ctx_t *
ten_extension_context_on_addon_create_extension_group_done_ctx_create(void) {
  ten_extension_context_on_addon_create_extension_group_done_ctx_t *self =
      TEN_MALLOC(sizeof(
          ten_extension_context_on_addon_create_extension_group_done_ctx_t));

  self->addon_context = NULL;
  self->extension_group = NULL;

  return self;
}

void ten_extension_context_on_addon_create_extension_group_done_ctx_destroy(
    ten_extension_context_on_addon_create_extension_group_done_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}
