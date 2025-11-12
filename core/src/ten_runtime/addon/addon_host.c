//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_host.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/base_dir.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

bool ten_addon_host_check_integrity(ten_addon_host_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");
  if (ten_signature_get(&self->signature) != TEN_ADDON_HOST_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_addon_host_deinit(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(self->addon, "Should not happen.");

  ten_env_close(self->ten_env);

  if (self->addon->on_destroy) {
    self->addon->on_destroy(self->addon);
  }

  ten_addon_host_destroy(self);
}

static void ten_addon_on_end_of_life(TEN_UNUSED ten_ref_t *ref,
                                     void *supervisee) {
  ten_addon_host_t *addon = supervisee;
  TEN_ASSERT(addon, "Invalid argument.");

  ten_addon_host_deinit(addon);
}

void ten_addon_host_init(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, TEN_ADDON_HOST_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  TEN_STRING_INIT(self->name);
  TEN_STRING_INIT(self->base_dir);

  ten_value_init_object_with_move(&self->manifest, NULL);
  ten_value_init_object_with_move(&self->property, NULL);

  ten_ref_init(&self->ref, self, ten_addon_on_end_of_life);
  self->ten_env = NULL;

  self->manifest_info = NULL;
  self->property_info = NULL;

  self->attached_app = NULL;
}

void ten_addon_host_destroy(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, 0);
  ten_sanitizer_thread_check_deinit(&self->thread_check);

  ten_string_deinit(&self->name);
  ten_string_deinit(&self->base_dir);

  ten_value_deinit(&self->manifest);
  ten_value_deinit(&self->property);

  self->attached_app = NULL;

  if (self->manifest_info) {
    ten_metadata_info_destroy(self->manifest_info);
    self->manifest_info = NULL;
  }
  if (self->property_info) {
    ten_metadata_info_destroy(self->property_info);
    self->property_info = NULL;
  }

  ten_env_destroy(self->ten_env);
  TEN_FREE(self);
}

const char *ten_addon_host_get_name(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_host_check_integrity(self, true), "Invalid argument.");
  return ten_string_get_raw_str(&self->name);
}

void ten_addon_host_find_and_set_base_dir(ten_addon_host_t *self,
                                          const char *start_path) {
  TEN_ASSERT(start_path && self && ten_addon_host_check_integrity(self, true),
             "Should not happen.");

  ten_string_t *base_dir =
      ten_find_base_dir(start_path, ten_addon_type_to_string(self->type),
                        ten_string_get_raw_str(&self->name));
  if (base_dir) {
    ten_string_copy(&self->base_dir, base_dir);
    ten_string_destroy(base_dir);
  } else {
    // If the addon's base dir cannot be found by searching upward through the
    // parent folders, simply trust the passed-in parameter as the addon’s base
    // dir.
    ten_string_set_from_c_str(&self->base_dir, start_path);
  }
}

const char *ten_addon_host_get_base_dir(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_addon_host_check_integrity(self, true), "Invalid argument.");

  return ten_string_get_raw_str(&self->base_dir);
}

ten_addon_context_t *ten_addon_context_create(void) {
  ten_addon_context_t *self = TEN_MALLOC(sizeof(ten_addon_context_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->addon_type = TEN_ADDON_TYPE_INVALID;
  TEN_STRING_INIT(self->addon_name);
  TEN_STRING_INIT(self->instance_name);

  self->flow = TEN_ADDON_CONTEXT_FLOW_INVALID;

  self->create_instance_done_cb = NULL;
  self->create_instance_done_cb_data = NULL;

  self->destroy_instance_done_cb = NULL;
  self->destroy_instance_done_cb_data = NULL;

  return self;
}

void ten_addon_context_set_creation_info(ten_addon_context_t *self,
                                         TEN_ADDON_TYPE addon_type,
                                         const char *addon_name,
                                         const char *instance_name) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(addon_name && instance_name, "Invalid argument.");

  self->addon_type = addon_type;
  ten_string_set_from_c_str(&self->addon_name, addon_name);
  ten_string_set_from_c_str(&self->instance_name, instance_name);
}

void ten_addon_context_destroy(ten_addon_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->addon_name);
  ten_string_deinit(&self->instance_name);

  TEN_FREE(self);
}

/**
 * @param ten Might be the ten of the 'engine', or the ten of an extension
 * thread (group).
 * @param cb The callback when the creation is completed. Because there might be
 * more than one extension threads to create extensions from the corresponding
 * extension addons simultaneously. So we can _not_ save the function pointer
 * of @a cb into @a ten, instead we need to pass the function pointer of @a cb
 * through a parameter.
 * @param cb_data The user data of @a cb. Refer the comments of @a cb for the
 * reason why we pass the pointer of @a cb_data through a parameter rather than
 * saving it into @a ten.
 *
 * @note We will save the pointers of @a cb and @a cb_data into a 'ten' object
 * later in the call flow when the 'ten' object at that time belongs to a more
 * specific scope, so that we can minimize the parameters count then.
 */
void ten_addon_host_create_instance_async(ten_addon_host_t *self,
                                          const char *name,
                                          ten_addon_context_t *addon_context) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(name, "Should not happen.");
  TEN_ASSERT(addon_context, "Should not happen.");

  if (self->addon->on_create_instance) {
    TEN_ASSERT(self->addon->on_create_instance, "Should not happen.");
    self->addon->on_create_instance(self->addon, self->ten_env, name,
                                    addon_context);
  } else {
    TEN_ASSERT(0,
               "Failed to create instance from %s, because it does not define "
               "create() function.",
               name);
  }
}

typedef struct ten_app_addon_host_destroy_instance_ctx_t {
  ten_addon_host_t *addon_host;
  void *instance;
  ten_addon_context_t *addon_context;
} ten_app_addon_host_destroy_instance_ctx_t;

// The owner thread of the addon is the app thread, so this function is called
// from the app thread.
static void ten_app_addon_host_destroy_instance(void *from, void *args) {
  ten_app_t *app = (ten_app_t *)from;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_app_addon_host_destroy_instance_ctx_t *ctx =
      (ten_app_addon_host_destroy_instance_ctx_t *)args;
  TEN_ASSERT(ctx, "Should not happen.");

  ten_addon_host_t *addon_host = ctx->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host, true),
             "Should not happen.");

  addon_host->addon->on_destroy_instance(addon_host->addon, addon_host->ten_env,
                                         ctx->instance, ctx->addon_context);

  TEN_FREE(ctx);
}

/**
 * @param ten Might be the ten of the 'engine', or the ten of an extension
 * thread(group).
 * @param cb The callback when the creation is completed. Because there might be
 * more than one extension threads to create extensions from the corresponding
 * extension addons simultaneously. So we can _not_ save the function pointer
 * of @a cb into @a ten, instead we need to pass the function pointer of @a cb
 * through a parameter.
 * @param cb_data The user data of @a cb. Refer the comments of @a cb for the
 * reason why we pass the pointer of @a cb_data through a parameter rather than
 * saving it into @a ten.
 *
 * @note We will save the pointers of @a cb and @a cb_data into a 'ten' object
 * later in the call flow when the 'ten' object at that time belongs to a more
 * specific scope, so that we can minimize the parameters count then.
 */
bool ten_addon_host_destroy_instance_async(ten_addon_host_t *self,
                                           void *instance,
                                           ten_addon_context_t *addon_context) {
  TEN_ASSERT(self, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function could be called on any thread. Therefore, we
  // will check within this function if it is on the app thread and handle it
  // appropriately.
  TEN_ASSERT(ten_addon_host_check_integrity(self, false), "Should not happen.");
  TEN_ASSERT(instance, "Should not happen.");

  ten_app_t *app = self->attached_app;
  TEN_ASSERT(app && ten_app_check_integrity(app, false), "Should not happen.");

  if (!self->addon->on_destroy_instance) {
    TEN_ASSERT(0,
               "Failed to destroy an instance from %s, because it does not "
               "define a destroy() function.",
               ten_string_get_raw_str(&self->name));
    return false;
  }

  if (ten_app_thread_call_by_me(app)) {
    self->addon->on_destroy_instance(self->addon, self->ten_env, instance,
                                     addon_context);
  } else {
    ten_app_addon_host_destroy_instance_ctx_t *ctx =
        TEN_MALLOC(sizeof(ten_app_addon_host_destroy_instance_ctx_t));
    TEN_ASSERT(ctx, "Failed to allocate memory.");

    ctx->addon_host = self;
    ctx->instance = instance;
    ctx->addon_context = addon_context;

    int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(app),
                                        ten_app_addon_host_destroy_instance,
                                        app, ctx);
    TEN_ASSERT(rc == 0, "Failed to post task.");
  }

  return true;
}

ten_addon_host_on_destroy_instance_ctx_t *
ten_addon_host_on_destroy_instance_ctx_create(
    ten_addon_host_t *self, void *instance,
    ten_env_addon_destroy_instance_done_cb_t cb, void *cb_data) {
  TEN_ASSERT(self && instance, "Should not happen.");

  ten_addon_host_on_destroy_instance_ctx_t *ctx =
      (ten_addon_host_on_destroy_instance_ctx_t *)TEN_MALLOC(
          sizeof(ten_addon_host_on_destroy_instance_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_host = self;
  ctx->instance = instance;
  ctx->cb = cb;
  ctx->cb_data = cb_data;

  return ctx;
}

void ten_addon_host_on_destroy_instance_ctx_destroy(
    ten_addon_host_on_destroy_instance_ctx_t *self) {
  TEN_ASSERT(self && self->addon_host && self->instance, "Should not happen.");
  TEN_FREE(self);
}

ten_addon_host_t *ten_addon_host_create(TEN_ADDON_TYPE type) {
  ten_addon_host_t *self =
      (ten_addon_host_t *)TEN_MALLOC(sizeof(ten_addon_host_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->type = type;
  ten_addon_host_init(self);

  return self;
}

void ten_addon_host_load_metadata(ten_addon_host_t *self, ten_env_t *ten_env) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env_get_attached_addon(ten_env) == self, "Should not happen.");

  self->manifest_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_MANIFEST, ten_env);
  self->property_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_PROPERTY, ten_env);

  if (self->addon->on_configure) {
    self->addon->on_configure(self->addon, ten_env);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_handle_manifest_info_when_on_configure_done(
      &self->manifest_info, NULL, &self->manifest, &err);
  if (!rc) {
    TEN_LOGW("Failed to load addon manifest data, FATAL ERROR");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  rc = ten_handle_property_info_when_on_configure_done(
      &self->property_info, NULL, &self->property, &err);
  if (!rc) {
    TEN_LOGW("Failed to load addon property data, FATAL ERROR");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  ten_value_t *manifest_name_value =
      ten_value_object_peek(&self->manifest, TEN_STR_NAME);

  const char *manifest_name = NULL;
  if (manifest_name_value) {
    manifest_name = ten_value_peek_raw_str(manifest_name_value, &err);
  }

  ten_error_deinit(&err);

  if (manifest_name) {
    TEN_ASSERT(manifest_name, "Should not happen.");

    if (ten_string_len(&self->name) &&
        !ten_string_is_equal_c_str(&self->name, manifest_name)) {
      TEN_LOGW(
          "The registered addon name (%s) is not equal to the name (%s) in "
          "the manifest",
          ten_string_get_raw_str(&self->name), manifest_name);

      // Get 'name' from manifest, and check the consistency between the name
      // specified in the argument, and the name specified in the manifest.
      //
      // The name in the manifest could be checked by the TEN store to ensure
      // the uniqueness of the name.
      TEN_ASSERT(0, "Should not happen.");
    }

    // If an addon defines an addon name in its manifest file, TEN runtime
    // would use that name instead of the name specified in the codes to
    // register it to the addon store.
    if (strlen(manifest_name)) {
      ten_string_set_from_c_str(&self->name, manifest_name);
    }
  }
}
