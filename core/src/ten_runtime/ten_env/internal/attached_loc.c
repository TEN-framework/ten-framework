//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/ten_env/ten_env.h"

const char *ten_env_get_attached_instance_name(ten_env_t *self,
                                               bool check_thread) {
  TEN_ASSERT(self && ten_env_check_integrity(self, check_thread),
             "Invalid argument.");

  switch (self->attach_to) {
  case TEN_ENV_ATTACH_TO_EXTENSION: {
    ten_extension_t *extension = ten_env_get_attached_extension(self);
    return ten_extension_get_name(extension, true);
  }
  case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
    ten_extension_group_t *extension_group =
        ten_env_get_attached_extension_group(self);
    return ten_extension_group_get_name(extension_group, true);
  }
  case TEN_ENV_ATTACH_TO_ENGINE: {
    ten_engine_t *engine = ten_env_get_attached_engine(self);
    return ten_engine_get_id(engine, true);
  }
  case TEN_ENV_ATTACH_TO_APP: {
    ten_app_t *app = ten_env_get_attached_app(self);
    return ten_app_get_uri(app);
  }
  case TEN_ENV_ATTACH_TO_ADDON: {
    ten_addon_host_t *addon_host = ten_env_get_attached_addon(self);
    return ten_addon_host_get_name(addon_host);
  }
  default:
    TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
    return NULL;
  }
}
