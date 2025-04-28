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
#include "include_internal/ten_runtime/common/base_dir.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

bool ten_addon_host_check_integrity(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  if (ten_signature_get(&self->signature) != TEN_ADDON_HOST_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_addon_host_deinit(ten_addon_host_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(self->addon, "Should not happen.");

  if (self->addon->on_deinit) {
    self->addon->on_deinit(self->addon, self->ten_env);
  } else {
    ten_env_on_deinit_done(self->ten_env, NULL);
  }
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

bool ten_addon_host_destroy_instance(ten_addon_host_t *self, ten_env_t *ten_env,
                                     void *instance) {
  TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, false),
             "Should not happen.");
  TEN_ASSERT(self && instance, "Should not happen.");
  TEN_ASSERT(self->addon->on_destroy_instance, "Should not happen.");

  self->addon->on_destroy_instance(self->addon, self->ten_env, instance, NULL);

  return true;
}

const char *ten_addon_host_get_name(ten_addon_host_t *self) {
  TEN_ASSERT(self && ten_addon_host_check_integrity(self), "Invalid argument.");
  return ten_string_get_raw_str(&self->name);
}

void ten_addon_host_find_and_set_base_dir(ten_addon_host_t *self,
                                          const char *start_path) {
  TEN_ASSERT(start_path && self && ten_addon_host_check_integrity(self),
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
  TEN_ASSERT(self && ten_addon_host_check_integrity(self), "Invalid argument.");
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
  TEN_ASSERT(ten_addon_host_check_integrity(self), "Should not happen.");
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
  TEN_ASSERT(ten_addon_host_check_integrity(self), "Should not happen.");
  TEN_ASSERT(instance, "Should not happen.");
  TEN_ASSERT(addon_context, "Should not happen.");

  if (self->addon->on_destroy_instance) {
    TEN_ASSERT(self->addon->on_destroy_instance, "Should not happen.");
    self->addon->on_destroy_instance(self->addon, self->ten_env, instance,
                                     addon_context);
  } else {
    TEN_ASSERT(0,
               "Failed to destroy an instance from %s, because it does not "
               "define a destroy() function.",
               ten_string_get_raw_str(&self->name));
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

void ten_addon_host_load_metadata(ten_addon_host_t *self, ten_env_t *ten_env,
                                  ten_addon_on_init_func_t on_init) {
  TEN_ASSERT(self && ten_addon_host_check_integrity(self),
             "Should not happen.");
  TEN_ASSERT(ten_env && ten_env_check_integrity(ten_env, true) &&
                 ten_env_get_attached_addon(ten_env) == self,
             "Should not happen.");

  self->manifest_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_MANIFEST, ten_env);
  self->property_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_PROPERTY, ten_env);

  if (on_init) {
    on_init(self->addon, ten_env);
  } else {
    ten_env_on_init_done(ten_env, NULL);
  }
}
