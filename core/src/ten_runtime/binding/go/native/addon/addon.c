//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/go/interface/ten_runtime/addon.h"

#include <stdint.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/binding/go/addon/addon.h"
#include "include_internal/ten_runtime/binding/go/extension/extension.h"
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/addon/addon_manager.h"
#include "ten_runtime/addon/extension/extension.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

void tenGoAddonOnInit(ten_go_handle_t go_addon, ten_go_handle_t go_ten_env);

void tenGoAddonOnDeinit(ten_go_handle_t go_addon, ten_go_handle_t go_ten_env);

void tenGoAddonCreateInstance(ten_go_handle_t go_addon,
                              ten_go_handle_t go_ten_env, const char *name,
                              void *context);

void tenGoAddonDestroyInstance(ten_go_handle_t go_instance);

void tenGoAddonOnDestroy(ten_go_handle_t go_addon);

void tenGoAddonManagerCallRegisterHandler(TEN_ADDON_TYPE addon_type,
                                          const char *addon_name,
                                          void *register_ctx);

bool ten_go_addon_check_integrity(ten_go_addon_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_GO_ADDON_SIGNATURE) {
    return false;
  }

  return true;
}

ten_go_handle_t ten_go_addon_go_handle(ten_go_addon_t *self) {
  TEN_ASSERT(self && ten_go_addon_check_integrity(self), "Should not happen.");

  return self->bridge.go_instance;
}

static void ten_go_addon_destroy(ten_go_addon_t *self) {
  TEN_ASSERT(self && ten_go_addon_check_integrity(self), "Should not happen.");

  ten_string_deinit(&self->addon_name);
  TEN_FREE(self);
}

void ten_go_addon_finalize(uintptr_t bridge_addr) {
  TEN_ASSERT(bridge_addr, "Invalid argument.");

  ten_go_addon_t *addon_bridge = (ten_go_addon_t *)bridge_addr;
  TEN_ASSERT(addon_bridge, "Invalid argument.");
  TEN_ASSERT(ten_go_addon_check_integrity(addon_bridge), "Invalid argument.");

  // The C part is disappear, so decrement the reference count to reflect this
  // fact.
  ten_go_bridge_destroy_c_part(&addon_bridge->bridge);

  // The addon is not used by GO world any more, just destroy it.
  ten_go_bridge_destroy_go_part(&addon_bridge->bridge);
}

static void ten_go_addon_on_init_helper(ten_addon_t *addon,
                                        ten_env_t *ten_env) {
  TEN_ASSERT(addon && ten_addon_check_integrity(addon), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge, "Invalid argument.");
  TEN_ASSERT(ten_go_addon_check_integrity(addon_bridge), "Invalid argument.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoAddonOnInit(ten_go_addon_go_handle(addon_bridge),
                   ten_go_ten_env_go_handle(ten_env_bridge));
}

static void ten_go_addon_on_deinit_helper(ten_addon_t *addon,
                                          ten_env_t *ten_env) {
  TEN_ASSERT(addon, "Invalid argument.");
  TEN_ASSERT(ten_addon_check_integrity(addon), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_env_check_integrity(ten_env, false), "Invalid argument.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge, "Invalid argument.");
  TEN_ASSERT(ten_go_addon_check_integrity(addon_bridge), "Invalid argument.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  tenGoAddonOnDeinit(ten_go_addon_go_handle(addon_bridge),
                     ten_go_ten_env_go_handle(ten_env_bridge));
}

static void ten_go_addon_create_extension_async_helper(ten_addon_t *addon,
                                                       ten_env_t *ten_env,
                                                       const char *name,
                                                       void *context) {
  TEN_ASSERT(addon, "Should not happen.");
  TEN_ASSERT(ten_addon_check_integrity(addon), "Should not happen.");
  TEN_ASSERT(name, "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge && ten_go_addon_check_integrity(addon_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  TEN_ASSERT(ten_env_bridge && ten_go_ten_env_check_integrity(ten_env_bridge),
             "Invalid argument.");

  tenGoAddonCreateInstance(ten_go_addon_go_handle(addon_bridge),
                           ten_go_ten_env_go_handle(ten_env_bridge), name,
                           context);
}

static void ten_go_addon_create_extension_group_async_helper(ten_addon_t *addon,
                                                             ten_env_t *ten_env,
                                                             const char *name,
                                                             void *context) {
  TEN_ASSERT(addon && ten_addon_check_integrity(addon) && name && ten_env,
             "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge && ten_go_addon_check_integrity(addon_bridge),
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  TEN_ASSERT(ten_env_bridge && ten_go_ten_env_check_integrity(ten_env_bridge),
             "Invalid argument.");

  tenGoAddonCreateInstance(ten_go_addon_go_handle(addon_bridge),
                           ten_go_ten_env_go_handle(ten_env_bridge), name,
                           context);
}

static void ten_go_addon_destroy_instance_helper(ten_addon_t *addon,
                                                 ten_env_t *ten_env,
                                                 void *instance,
                                                 void *context) {
  TEN_ASSERT(addon && ten_addon_check_integrity(addon) && instance && ten_env,
             "Should not happen.");
  TEN_ASSERT(ten_env->attach_to == TEN_ENV_ATTACH_TO_ADDON,
             "Should not happen.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge && ten_go_addon_check_integrity(addon_bridge),
             "Should not happen.");

  ten_go_handle_t instance_handle = -1;
  switch (addon_bridge->type) {
  case TEN_ADDON_TYPE_EXTENSION: {
    ten_extension_t *extension = (ten_extension_t *)instance;
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid argument.");

    // Because the extension increases the reference count of the
    // corresponding `addon_host` when it is created, the reference count must
    // be decreased when the extension is destroyed.
    ten_addon_host_t *addon_host = extension->addon_host;
    TEN_ASSERT(addon_host && ten_addon_host_check_integrity(addon_host),
               "Invalid argument.");
    ten_ref_dec_ref(&addon_host->ref);
    extension->addon_host = NULL;

    ten_go_extension_t *extension_bridge =
        ten_binding_handle_get_me_in_target_lang(
            (ten_binding_handle_t *)extension);
    TEN_ASSERT(
        extension_bridge && ten_go_extension_check_integrity(extension_bridge),
        "Invalid argument.");

    instance_handle = ten_go_extension_go_handle(extension_bridge);
    break;
  }

  default:
    TEN_ASSERT(0, "Not support.");
    break;
  }

  if (instance_handle > 0) {
    tenGoAddonDestroyInstance(instance_handle);
  }

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static void ten_go_addon_on_destroy(ten_addon_t *addon) {
  TEN_ASSERT(addon, "Should not happen.");
  TEN_ASSERT(ten_addon_check_integrity(addon), "Should not happen.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)addon->binding_handle.me_in_target_lang;
  TEN_ASSERT(addon_bridge && ten_go_addon_check_integrity(addon_bridge),
             "Should not happen.");

  tenGoAddonOnDestroy(addon_bridge->bridge.go_instance);
}

static ten_go_addon_t *ten_go_addon_register(const void *addon_name,
                                             int addon_name_len,
                                             uintptr_t go_addon,
                                             TEN_ADDON_TYPE addon_type,
                                             void *register_ctx) {
  TEN_ASSERT(addon_name && addon_name_len > 0, "Invalid argument.");

  ten_go_addon_t *addon_bridge =
      (ten_go_addon_t *)TEN_MALLOC(sizeof(ten_go_addon_t));
  TEN_ASSERT(addon_bridge, "Failed to allocate memory.");

  ten_signature_set(&addon_bridge->signature, TEN_GO_ADDON_SIGNATURE);

  addon_bridge->bridge.go_instance = go_addon;

  addon_bridge->bridge.sp_ref_by_go =
      ten_shared_ptr_create(addon_bridge, ten_go_addon_destroy);
  addon_bridge->bridge.sp_ref_by_c =
      ten_shared_ptr_clone(addon_bridge->bridge.sp_ref_by_go);

  addon_bridge->type = addon_type;
  ten_string_init_from_c_str_with_size(&addon_bridge->addon_name, addon_name,
                                       addon_name_len);

  switch (addon_type) {
  case TEN_ADDON_TYPE_EXTENSION:
    ten_addon_init(&addon_bridge->c_addon, ten_go_addon_on_init_helper,
                   ten_go_addon_on_deinit_helper,
                   ten_go_addon_create_extension_async_helper,
                   ten_go_addon_destroy_instance_helper,
                   ten_go_addon_on_destroy);
    break;

  case TEN_ADDON_TYPE_EXTENSION_GROUP:
    ten_addon_init(&addon_bridge->c_addon, ten_go_addon_on_init_helper,
                   ten_go_addon_on_deinit_helper,
                   ten_go_addon_create_extension_group_async_helper,
                   ten_go_addon_destroy_instance_helper, NULL);
    break;

  default:
    TEN_ASSERT(0, "Not support.");
    break;
  }

  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)&addon_bridge->c_addon, addon_bridge);

  switch (addon_type) {
  case TEN_ADDON_TYPE_EXTENSION:
    // The Go addon is not dynamically loaded; it is statically compiled
    // directly into the Go app. As a result, the base_dir captured by the Go
    // code in the addon during compilation reflects the base_dir at the time
    // of compilation, which may not correspond to a path under
    // `ten_packages/`. Therefore, the Go addon does not have the capability
    // to specify the base_dir via its own code. Instead, it relies on the C
    // runtime at runtime to first locate the app's base_dir and then
    // determine the correct runtime base_dir for the Go addon by combining
    // the path `ten_packages/<addon_type>/<addon_name>`. This behavior is
    // represented by specifying a special string,
    // `TEN_STR_ADDON_BASE_DIR_FIND_FROM_APP_BASE_DIR`.
    ten_addon_register_extension(
        ten_string_get_raw_str(&addon_bridge->addon_name),
        TEN_STR_ADDON_BASE_DIR_FIND_FROM_APP_BASE_DIR, &addon_bridge->c_addon,
        register_ctx);
    break;

  default:
    TEN_ASSERT(0, "Not support.");
    break;
  }

  return addon_bridge;
}

ten_go_error_t ten_go_addon_register_extension(const void *addon_name,
                                               int addon_name_len,
                                               uintptr_t go_addon,
                                               uintptr_t register_ctx,
                                               uintptr_t *bridge_addr) {
  TEN_ASSERT(addon_name && addon_name_len > 0 && go_addon && bridge_addr,
             "Invalid argument.");

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_go_addon_t *addon_bridge = ten_go_addon_register(
      addon_name, addon_name_len, go_addon, TEN_ADDON_TYPE_EXTENSION,
      // NOLINTNEXTLINE(performance-no-int-to-ptr)
      (void *)register_ctx);

  *bridge_addr = (uintptr_t)addon_bridge;

  return cgo_error;
}

static void ten_go_addon_register_func(TEN_ADDON_TYPE addon_type,
                                       ten_string_t *addon_name,
                                       void *register_ctx,
                                       TEN_UNUSED void *user_data) {
  tenGoAddonManagerCallRegisterHandler(
      addon_type, ten_string_get_raw_str(addon_name), register_ctx);
}

ten_go_error_t ten_go_addon_manager_add_extension_addon(const void *addon_name,
                                                        int addon_name_len) {
  TEN_ASSERT(addon_name && addon_name_len > 0, "Invalid argument.");

  ten_string_t addon_name_str;
  ten_string_init_from_c_str_with_size(&addon_name_str, addon_name,
                                       addon_name_len);

  ten_addon_manager_t *addon_manager = ten_addon_manager_get_instance();

  ten_go_error_t cgo_error;
  ten_go_error_init_with_error_code(&cgo_error, TEN_ERROR_CODE_OK);

  ten_error_t error;
  TEN_ERROR_INIT(error);

  bool rc = ten_addon_manager_add_addon(
      addon_manager, "extension", ten_string_get_raw_str(&addon_name_str),
      ten_go_addon_register_func, NULL, &error);
  if (!rc) {
    ten_go_error_from_error(&cgo_error, &error);
  }

  ten_error_deinit(&error);
  ten_string_deinit(&addon_name_str);

  return cgo_error;
}
