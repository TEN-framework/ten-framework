//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_env/internal/on_xxx_done.h"

#include "include_internal/ten_runtime/addon/addon_loader/ten_env/on_xxx.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/ten_env/on_xxx.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/on_xxx.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/on_xxx.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_context/ten_env/on_xxx.h"
#include "include_internal/ten_runtime/extension_group/builtin/builtin_extension_group.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/ten_env/on_xxx_done.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

bool ten_env_on_configure_done(ten_env_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    return ten_extension_on_configure_done(self);

  case TEN_ENV_ATTACH_TO_APP:
    ten_app_on_configure_done(self);
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
  case TEN_ENV_ATTACH_TO_ADDON:
    TEN_ASSERT(0, "Handle these types.");
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}

bool ten_env_on_init_done(ten_env_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION:
    return ten_extension_on_init_done(self);

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    ten_extension_group_on_init_done(self);
    break;

  case TEN_ENV_ATTACH_TO_APP:
    ten_app_on_init_done(self);
    break;

  case TEN_ENV_ATTACH_TO_ADDON:
    ten_addon_on_init_done(self);
    break;

  case TEN_ENV_ATTACH_TO_ADDON_LOADER:
    ten_addon_loader_on_init_done(self);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}

bool ten_env_on_deinit_done(ten_env_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_ADDON:
    return ten_addon_on_deinit_done(self);

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    return ten_extension_group_on_deinit_done(self);

  case TEN_ENV_ATTACH_TO_EXTENSION:
    return ten_extension_on_deinit_done(self);

  case TEN_ENV_ATTACH_TO_APP:
    return ten_app_on_deinit_done(self);

  case TEN_ENV_ATTACH_TO_ADDON_LOADER:
    return ten_addon_loader_on_deinit_done(self);

  default:
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
}

bool ten_env_on_create_extensions_done(
    ten_env_t *self, ten_extension_group_create_extensions_done_ctx_t *ctx,
    TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  ten_extension_group_t *extension_group =
      ten_env_get_attached_extension_group(self);
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_extension_group_on_create_extensions_done(extension_group, &ctx->results);

  return true;
}

bool ten_env_on_destroy_extensions_done(ten_env_t *self,
                                        TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  ten_extension_group_t *extension_group =
      ten_env_get_attached_extension_group(self);
  TEN_ASSERT(extension_group, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Should not happen.");

  ten_extension_group_on_destroy_extensions_done(extension_group);

  return true;
}

bool ten_env_on_create_instance_done(ten_env_t *self, void *instance,
                                     void *context,
                                     TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_ADDON:
    ten_addon_on_create_instance_done(self, instance, context);
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}

bool ten_env_on_destroy_instance_done(ten_env_t *self, void *context,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(
      ten_env_check_integrity(
          self, self->attach_to != TEN_ENV_ATTACH_TO_ADDON ? true : false),
      "Invalid use of ten_env %p.", self);

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_ADDON:
    ten_addon_on_destroy_instance_done(self, context);
    break;

  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP:
    ten_extension_group_on_addon_destroy_extension_done(self, context);
    break;

  default:
    TEN_ASSERT(0, "Should not happen: %d.", self->attach_to);
    break;
  }

  return true;
}

bool ten_env_on_load_addon_done(ten_env_t *self, void *context,
                                TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON_LOADER,
             "Should not happen.");

  return ten_addon_loader_on_load_addon_done(self, context);
}

bool ten_env_on_start_done(ten_env_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION,
             "Should not happen.");

  return ten_extension_on_start_done(self);
}

bool ten_env_on_stop_done(ten_env_t *self, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, true), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION,
             "Should not happen.");

  return ten_extension_on_stop_done(self);
}
