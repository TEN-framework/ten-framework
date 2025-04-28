//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/extension/extension.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/extension/extension.h"
#include "ten_runtime/ten.h"
#include "ten_utils/macro/check.h"

void ten_go_ten_env_on_create_instance_done(uintptr_t bridge_addr,
                                            uintptr_t instance_bridge_addr,
                                            uintptr_t context_addr) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self && ten_go_ten_env_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(context_addr, "Invalid argument.");

  ten_extension_t *c_extension_or_extension_group = NULL;
  if (instance_bridge_addr) {
    ten_go_extension_t *extension_bridge =
        ten_go_extension_reinterpret(instance_bridge_addr);
    TEN_ASSERT(ten_go_extension_check_integrity(extension_bridge),
               "Should not happen.");
    c_extension_or_extension_group =
        ten_go_extension_c_extension(extension_bridge);
  }

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {});

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_env_on_create_instance_done(self->c_ten_env,
                                            c_extension_or_extension_group,
                                            (void *)context_addr, &err);
  TEN_ASSERT(rc, "Should not happen.");

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return;
}
