//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/builtin/builtin_extension_group.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_addon_and_instance_name_pair.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/metadata.h"
#include "include_internal/ten_runtime/ten_env/on_xxx_done.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/msg/msg.h"
#include "ten_runtime/ten.h"
#include "ten_runtime/ten_env/internal/log.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

ten_extension_group_create_extensions_done_ctx_t *
ten_extension_group_create_extensions_done_ctx_create(void) {
  ten_extension_group_create_extensions_done_ctx_t *self =
      TEN_MALLOC(sizeof(ten_extension_group_create_extensions_done_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_list_init(&self->results);

  return self;
}

void ten_extension_group_create_extensions_done_ctx_destroy(
    ten_extension_group_create_extensions_done_ctx_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_list_clear(&self->results);

  TEN_FREE(self);
}

// Because the creation process for extensions is asynchronous, it is necessary
// to check whether the number of extensions already created has reached the
// initially set target each time an extension is successfully created. If the
// target is met, it means that all the required extensions for this extension
// group have been successfully created.
static void on_addon_create_extension_done(ten_env_t *ten_env,
                                           ten_extension_t *extension,
                                           void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(
      ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
      "Invalid argument.");

  ten_addon_create_extension_done_ctx_t *create_extension_done_ctx =
      (ten_addon_create_extension_done_ctx_t *)cb_data;
  TEN_ASSERT(create_extension_done_ctx, "Should not happen.");

  ten_extension_group_create_extensions_done_ctx_t *create_extensions_done_ctx =
      create_extension_done_ctx->create_extensions_done_ctx;

  ten_list_t *results = &create_extensions_done_ctx->results;
  TEN_ASSERT(results, "Should not happen.");

  ten_extension_group_t *extension_group = ten_env_get_attached_target(ten_env);
  TEN_ASSERT(extension_group, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Invalid argument.");

  if (extension) {
    // Successful to create the specified extension.

    TEN_LOGI(
        "Success to create extension %s",
        ten_string_get_raw_str(&create_extension_done_ctx->extension_name));

    TEN_ASSERT(ten_extension_check_integrity(extension, true),
               "Invalid argument.");

    ten_list_push_ptr_back(results, extension, NULL);
  } else {
    // Failed to create the specified extension.

    TEN_LOGE(
        "Failed to create extension %s",
        ten_string_get_raw_str(&create_extension_done_ctx->extension_name));

    // Use a value that is absolutely incorrect to represent an extension that
    // could not be successfully created. This ensures that the final count in
    // the `results` matches the expected number; otherwise, it would get stuck,
    // endlessly waiting for the desired number of extensions to be created. In
    // later steps, these special, unsuccessfully created extension instances
    // will be removed.
    ten_list_push_ptr_back(results, TEN_EXTENSION_UNSUCCESSFULLY_CREATED, NULL);
  }

  if (ten_list_size(results) ==
      ten_list_size(
          ten_extension_group_get_extension_addon_and_instance_name_pairs(
              extension_group))) {
    // Notify the builtin extension group that all extensions have been created.

    ten_env_on_create_extensions_done(
        ten_extension_group_get_ten_env(extension_group),
        create_extensions_done_ctx, NULL);

    ten_extension_group_create_extensions_done_ctx_destroy(
        create_extensions_done_ctx);
  }

  ten_addon_create_extension_done_ctx_destroy(create_extension_done_ctx);
}

static void on_addon_destroy_instance_done(ten_env_t *ten_env,
                                           TEN_UNUSED void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(
      ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
      "Invalid argument.");

  ten_extension_group_t *extension_group = ten_env_get_attached_target(ten_env);
  TEN_ASSERT(extension_group, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Invalid argument.");

  // We modify 'extensions_cnt_of_being_destroyed' on the extension thread, so
  // it's thread safe.
  if (ten_extension_group_decrement_extension_cnt_of_being_destroyed(
          extension_group) == 0) {
    ten_env_on_destroy_extensions_done(ten_env, NULL);
  }
}

static void ten_builtin_extension_group_on_init(ten_extension_group_t *self,
                                                ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  ten_env_on_init_done(ten_env, NULL);
}

static void ten_builtin_extension_group_on_deinit(ten_extension_group_t *self,
                                                  ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  ten_env_on_deinit_done(ten_env, NULL);
}

static void ten_builtin_extension_group_on_create_extensions(
    ten_extension_group_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  ten_extension_group_create_extensions_done_ctx_t *create_extensions_done_ctx =
      ten_extension_group_create_extensions_done_ctx_create();

  if (ten_list_is_empty(
          ten_extension_group_get_extension_addon_and_instance_name_pairs(
              self))) {
    // This extension group is empty, so it can be considered that all the
    // required extensions have been successfully created.

    TEN_LOGI(
        "%s is a group without any extensions, so it is considered that all "
        "extensions have been successfully created.",
        ten_string_get_raw_str(&self->name));

    ten_env_on_create_extensions_done(ten_env, create_extensions_done_ctx,
                                      NULL);
    ten_extension_group_create_extensions_done_ctx_destroy(
        create_extensions_done_ctx);

    return;
  }

  // Get the information of all the extensions which this extension group should
  // create.
  ten_list_foreach (
      ten_extension_group_get_extension_addon_and_instance_name_pairs(self),
      iter) {
    ten_extension_addon_and_instance_name_pair_t *extension_name_info =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(extension_name_info, "Invalid argument.");

    ten_string_t *extension_addon_name = &extension_name_info->addon_name;
    ten_string_t *extension_instance_name = &extension_name_info->instance_name;

    ten_addon_create_extension_done_ctx_t *create_extension_done_ctx =
        ten_addon_create_extension_done_ctx_create(
            ten_string_get_raw_str(extension_instance_name),
            create_extensions_done_ctx);

    bool res = ten_addon_create_extension(
        ten_env, ten_string_get_raw_str(extension_addon_name),
        ten_string_get_raw_str(extension_instance_name),
        (ten_env_addon_create_instance_done_cb_t)on_addon_create_extension_done,
        create_extension_done_ctx, NULL);

    if (!res) {
      TEN_LOGE("Failed to find the addon for extension %s",
               ten_string_get_raw_str(extension_addon_name));

      ten_error_set(&self->err_before_ready, TEN_ERROR_CODE_INVALID_GRAPH,
                    "Failed to find the addon for extension %s",
                    ten_string_get_raw_str(extension_addon_name));

      // Unable to create the desired extension, proceeding with the failure
      // path. The callback of `ten_addon_create_extension` will not be invoked
      // when `res` is `false`, so we need to call the callback function here to
      // ensure the process can continue.
      on_addon_create_extension_done(ten_env, NULL, create_extension_done_ctx);
    }
  }
}

static void ten_builtin_extension_group_on_destroy_extensions(
    ten_extension_group_t *self, ten_env_t *ten_env, ten_list_t extensions) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  if (ten_list_size(&extensions) == 0) {
    ten_env_on_destroy_extensions_done(ten_env, NULL);
    return;
  }

  ten_extension_group_set_extension_cnt_of_being_destroyed(
      self, ten_list_size(&extensions));

  ten_list_foreach (&extensions, iter) {
    ten_extension_t *extension = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
               "Invalid argument.");

    ten_addon_destroy_extension(ten_env, extension,
                                on_addon_destroy_instance_done, NULL, NULL);
  }
}

void ten_builtin_extension_group_addon_on_init(TEN_UNUSED ten_addon_t *addon,
                                               ten_env_t *ten_env) {
  bool result = ten_env_init_manifest_from_json(ten_env,
                                                // clang-format off
                            "{\
                              \"type\": \"extension_group\",\
                              \"name\": \"default_extension_group\",\
                              \"version\": \"1.0.0\"\
                             }",
                                                // clang-format on
                                                NULL);
  TEN_ASSERT(result, "Should not happen.");

  ten_env_on_init_done(ten_env, NULL);
}

void ten_builtin_extension_group_addon_create_instance(ten_addon_t *addon,
                                                       ten_env_t *ten_env,
                                                       const char *name,
                                                       void *context) {
  TEN_ASSERT(addon && name, "Invalid argument.");

  ten_extension_group_t *ext_group = ten_extension_group_create(
      name, NULL, ten_builtin_extension_group_on_init,
      ten_builtin_extension_group_on_deinit,
      ten_builtin_extension_group_on_create_extensions,
      ten_builtin_extension_group_on_destroy_extensions);

  ten_env_on_create_instance_done(ten_env, ext_group, context, NULL);
}

void ten_builtin_extension_group_addon_destroy_instance(
    TEN_UNUSED ten_addon_t *addon, ten_env_t *ten_env, void *_extension_group,
    void *context) {
  ten_extension_group_t *extension_group =
      (ten_extension_group_t *)_extension_group;
  TEN_ASSERT(extension_group, "Invalid argument.");

  ten_extension_group_destroy(extension_group);

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static ten_addon_t builtin_extension_group_addon = {
    NULL,
    TEN_ADDON_SIGNATURE,
    ten_builtin_extension_group_addon_on_init,
    NULL,
    ten_builtin_extension_group_addon_create_instance,
    ten_builtin_extension_group_addon_destroy_instance,
    NULL,
    NULL,
};

// Addon registration phase 2: actually registering the addon into the addon
// store.
static void ten_builtin_extension_group_register_handler(
    TEN_UNUSED TEN_ADDON_TYPE addon_type, TEN_UNUSED ten_string_t *addon_name,
    void *register_ctx, TEN_UNUSED void *user_data) {
  ten_addon_register_extension_group(TEN_STR_DEFAULT_EXTENSION_GROUP, NULL,
                                     &builtin_extension_group_addon,
                                     register_ctx);
}

// This is the phase 1 of the addon registration process: adding a function,
// which will perform the actual registration in the phase 2, into the
// `addon_manager`.
void ten_addon_manager_add_builtin_extension_group(
    ten_addon_manager_t *manager) {
  TEN_ASSERT(manager, "Invalid argument.");

  ten_addon_manager_add_addon(
      manager, "extension_group", TEN_STR_DEFAULT_EXTENSION_GROUP,
      ten_builtin_extension_group_register_handler, NULL, NULL);
}
