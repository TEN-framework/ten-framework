//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/on_xxx.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/on_xxx.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/macro/check.h"

void ten_env_add_ten_proxy(ten_env_t *self, ten_env_proxy_t *ten_env_proxy) {
  TEN_ASSERT(self && ten_env_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env_proxy, "Invalid argument.");
  TEN_ASSERT(ten_env_proxy_check_integrity(ten_env_proxy), "Invalid argument.");

  ten_list_push_ptr_back(&self->ten_proxy_list, ten_env_proxy, NULL);
}

void ten_env_delete_ten_proxy(ten_env_t *self, ten_env_proxy_t *ten_env_proxy) {
  TEN_ASSERT(self && ten_env_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env_proxy, "Invalid argument.");
  TEN_ASSERT(ten_env_proxy_check_integrity(ten_env_proxy), "Invalid argument.");

  bool found = ten_list_remove_ptr(&self->ten_proxy_list, ten_env_proxy);
  TEN_ASSERT(found, "Should not happen.");

  if (ten_list_is_empty(&self->ten_proxy_list)) {
    switch (self->attach_to) {
    case TEN_ENV_ATTACH_TO_EXTENSION: {
      ten_extension_t *extension = self->attached_target.extension;
      TEN_ASSERT(extension, "Should not happen.");
      TEN_ASSERT(ten_extension_check_integrity(extension, true),
                 "Should not happen.");

      if (extension->state == TEN_EXTENSION_STATE_ON_DEINIT_DONE) {
        ten_extension_on_ten_env_proxy_released(self);
      }
      break;
    }

    case TEN_ENV_ATTACH_TO_EXTENSION_GROUP: {
      ten_extension_group_t *extension_group =
          self->attached_target.extension_group;
      TEN_ASSERT(extension_group, "Should not happen.");
      TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
                 "Should not happen.");

      if (extension_group->state == TEN_EXTENSION_GROUP_STATE_DEINIT_DONE) {
        ten_extension_group_on_ten_env_proxy_released(self);
      }
      break;
    }

    case TEN_ENV_ATTACH_TO_APP: {
      ten_app_t *app = self->attached_target.app;
      TEN_ASSERT(app, "Should not happen.");
      TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

      if (ten_app_is_closing(app)) {
        ten_app_on_ten_env_proxy_released(self);
      }
      break;
    }

    default:
      TEN_ASSERT(0, "Handle more types: %d", self->attach_to);
      break;
    }
  }
}
