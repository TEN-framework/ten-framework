//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_manager.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/common/common.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/memory.h"

ten_addon_manager_t *ten_addon_manager_get_instance(void) {
  static ten_addon_manager_t *instance = NULL;
  static ten_mutex_t *init_mutex = NULL;

  if (!init_mutex) {
    init_mutex = ten_mutex_create();
    TEN_ASSERT(init_mutex, "Failed to create initialization mutex.");
  }

  ten_mutex_lock(init_mutex);

  if (!instance) {
    instance = (ten_addon_manager_t *)ten_malloc(sizeof(ten_addon_manager_t));
    TEN_ASSERT(instance, "Failed to allocate memory for ten_addon_manager_t.");

    ten_list_init(&instance->registry);

    instance->rwlock = ten_rwlock_create(TEN_RW_DEFAULT_FAIRNESS);
    TEN_ASSERT(instance->rwlock, "Failed to create addon manager rwlock.");

    instance->app = NULL;
  }

  ten_mutex_unlock(init_mutex);

  return instance;
}

void ten_addon_manager_destroy(ten_addon_manager_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_rwlock_destroy(self->rwlock);
  ten_list_destroy(&self->registry);
}

static void ten_addon_registration_destroy(void *ptr) {
  ten_addon_registration_t *reg = (ten_addon_registration_t *)ptr;
  if (reg) {
    ten_string_deinit(&reg->addon_name);

    TEN_FREE(reg);
  }
}

bool ten_addon_manager_add_addon(ten_addon_manager_t *self,
                                 const char *addon_type_str,
                                 const char *addon_name,
                                 ten_addon_registration_func_t func,
                                 void *context, ten_error_t *error) {
  TEN_ASSERT(self && addon_name && func, "Invalid argument.");

  TEN_ADDON_TYPE addon_type = ten_addon_type_from_string(addon_type_str);
  if (addon_type == TEN_ADDON_TYPE_INVALID) {
    TEN_LOGE("Invalid addon type: %s", addon_type_str);
    if (error) {
      ten_error_set(error, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Invalid addon type: %s", addon_type_str);
    }
    return false;
  }

  ten_rwlock_lock(self->rwlock, 0);

  // Check if addon with the same name already exists.
  bool exists = false;

  ten_list_foreach (&self->registry, iter) {
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(iter.node);
    if (reg) {
      // Compare both addon type and addon name.
      if (reg->addon_type == addon_type &&
          ten_string_is_equal_c_str(&reg->addon_name, addon_name)) {
        exists = true;
        break;
      }
    }
  }

  if (!exists) {
    // Create a new ten_addon_registration_t.
    ten_addon_registration_t *reg = (ten_addon_registration_t *)TEN_MALLOC(
        sizeof(ten_addon_registration_t));
    TEN_ASSERT(reg, "Failed to allocate memory for ten_addon_registration_t.");

    reg->addon_type = addon_type;
    ten_string_init_from_c_str_with_size(&reg->addon_name, addon_name,
                                         strlen(addon_name));
    reg->func = func;
    reg->context = context;

    // Add to the registry.
    ten_list_push_ptr_back(&self->registry, reg,
                           ten_addon_registration_destroy);
  } else {
    // Handle the case where the addon is already added.
    // For now, log a warning.
    TEN_LOGW("Addon '%s:%s' is already registered", addon_type_str, addon_name);
  }

  ten_rwlock_unlock(self->rwlock, 0);

  return true;
}

bool ten_addon_is_registered(ten_addon_register_ctx_t *register_ctx,
                             TEN_ADDON_TYPE addon_type,
                             const char *addon_name) {
  TEN_ASSERT(register_ctx, "Invalid argument.");
  TEN_ASSERT(addon_name, "Invalid argument.");

  ten_app_t *app = register_ctx->app;
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  return ten_addon_store_find_by_type(app, addon_type, addon_name) != NULL;
}

typedef struct ten_addon_manager_register_context_t {
  ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered;
  void *cb_data;
  size_t expected_count;
  size_t registered_count;
} ten_addon_manager_register_context_t;

static ten_addon_manager_register_context_t *
ten_addon_manager_register_context_create(
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data, size_t expected_count) {
  ten_addon_manager_register_context_t *ctx =
      (ten_addon_manager_register_context_t *)TEN_MALLOC(
          sizeof(ten_addon_manager_register_context_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->on_all_addons_registered = on_all_addons_registered;
  ctx->cb_data = cb_data;
  ctx->expected_count = expected_count;
  ctx->registered_count = 0;

  return ctx;
}

static void ten_addon_manager_register_context_destroy(
    ten_addon_manager_register_context_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");
  TEN_FREE(ctx);
}

static void ten_addon_manager_on_addon_registered(void *register_ctx,
                                                  void *user_data) {
  ten_addon_manager_register_context_t *ctx =
      (ten_addon_manager_register_context_t *)user_data;
  TEN_ASSERT(ctx, "Invalid argument.");
  TEN_ASSERT(ctx->on_all_addons_registered, "Invalid argument.");

  ctx->registered_count++;
  if (ctx->registered_count == ctx->expected_count) {
    ctx->on_all_addons_registered(register_ctx, ctx->cb_data);

    ten_addon_manager_register_context_destroy(ctx);
  }
}

void ten_addon_manager_register_all_addons(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_rwlock_lock(self->rwlock, 1);

  ten_list_t addons_to_register = TEN_LIST_INIT_VAL;

  ten_list_foreach (&self->registry, iter) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);
    if (reg && reg->func) {
      ten_list_push_ptr_back(&addons_to_register, reg, NULL);
    }
  }

  ten_rwlock_unlock(self->rwlock, 1);

  if (ten_list_is_empty(&addons_to_register)) {
    on_all_addons_registered(register_ctx, cb_data);
    return;
  }

  // Create a register context.
  ten_addon_manager_register_context_t *ctx =
      ten_addon_manager_register_context_create(
          on_all_addons_registered, cb_data,
          ten_list_size(&addons_to_register));

  ten_list_iterator_t iter = ten_list_begin(&addons_to_register);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func) {
      reg->func(reg, ten_addon_manager_on_addon_registered, register_ctx, ctx);
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_list_clear(&addons_to_register);
}

void ten_addon_manager_register_all_addon_loaders(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->app, "Invalid argument.");
  TEN_ASSERT(on_all_addons_registered, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Invalid argument.");

  ten_list_t addons_to_register = TEN_LIST_INIT_VAL;

  ten_rwlock_lock(self->rwlock, 1);

  ten_list_iterator_t iter = ten_list_begin(&self->registry);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func && reg->addon_type == TEN_ADDON_TYPE_ADDON_LOADER) {
      // Check if the addon loader is already registered.
      ten_addon_host_t *addon_host = ten_addon_store_find_by_type(
          self->app, TEN_ADDON_TYPE_ADDON_LOADER,
          ten_string_get_raw_str(&reg->addon_name));
      if (!addon_host) {
        ten_list_push_ptr_back(&addons_to_register, reg, NULL);
      }
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_rwlock_unlock(self->rwlock, 1);

  if (ten_list_is_empty(&addons_to_register)) {
    on_all_addons_registered(register_ctx, cb_data);
    return;
  }

  // Create a register context.
  ten_addon_manager_register_context_t *ctx =
      ten_addon_manager_register_context_create(
          on_all_addons_registered, cb_data,
          ten_list_size(&addons_to_register));

  iter = ten_list_begin(&addons_to_register);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func && reg->addon_type == TEN_ADDON_TYPE_ADDON_LOADER) {
      // Check if the addon loader is already registered.
      ten_addon_host_t *addon_host = ten_addon_store_find_by_type(
          self->app, TEN_ADDON_TYPE_ADDON_LOADER,
          ten_string_get_raw_str(&reg->addon_name));
      if (!addon_host) {
        reg->func(reg, ten_addon_manager_on_addon_registered, register_ctx,
                  ctx);
      }
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_list_clear(&addons_to_register);
}

void ten_addon_manager_register_all_protocols(
    ten_addon_manager_t *self, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Invalid argument.");

  ten_list_t addons_to_register = TEN_LIST_INIT_VAL;

  ten_rwlock_lock(self->rwlock, 1);

  ten_list_iterator_t iter = ten_list_begin(&self->registry);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func && reg->addon_type == TEN_ADDON_TYPE_PROTOCOL) {
      ten_list_push_ptr_back(&addons_to_register, reg, NULL);
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_rwlock_unlock(self->rwlock, 1);

  if (ten_list_is_empty(&addons_to_register)) {
    on_all_addons_registered(register_ctx, cb_data);
    return;
  }

  // Create a register context.
  ten_addon_manager_register_context_t *ctx =
      ten_addon_manager_register_context_create(
          on_all_addons_registered, cb_data,
          ten_list_size(&addons_to_register));

  iter = ten_list_begin(&addons_to_register);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func && reg->addon_type == TEN_ADDON_TYPE_PROTOCOL) {
      // Check if the protocol is already registered.
      ten_addon_host_t *addon_host = ten_addon_store_find_by_type(
          self->app, TEN_ADDON_TYPE_PROTOCOL,
          ten_string_get_raw_str(&reg->addon_name));
      if (!addon_host) {
        reg->func(reg, ten_addon_manager_on_addon_registered, register_ctx,
                  ctx);
      }
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_list_clear(&addons_to_register);
}

bool ten_addon_manager_register_specific_addon(
    ten_addon_manager_t *self, TEN_ADDON_TYPE addon_type,
    const char *addon_name, void *register_ctx,
    ten_addon_manager_on_all_addons_registered_func_t on_all_addons_registered,
    void *cb_data) {
  TEN_ASSERT(self && addon_name, "Invalid argument.");

  ten_addon_registration_t *reg = NULL;

  ten_rwlock_lock(self->rwlock, 1);

  // Check if the specific addon exists.
  ten_list_foreach (&self->registry, iter) {
    ten_addon_registration_t *node =
        (ten_addon_registration_t *)ten_ptr_listnode_get(iter.node);
    if (node && node->addon_type == addon_type &&
        ten_string_is_equal_c_str(&node->addon_name, addon_name)) {
      reg = node;
      break;
    }
  }

  ten_rwlock_unlock(self->rwlock, 1);

  if (!reg) {
    TEN_LOGI("Unable to find '%s:%s' in registry",
             ten_addon_type_to_string(addon_type), addon_name);
    return false;
  }

  // Create a register context.
  ten_addon_manager_register_context_t *ctx =
      ten_addon_manager_register_context_create(on_all_addons_registered,
                                                cb_data, 1);

  reg->func(reg, ten_addon_manager_on_addon_registered, register_ctx, ctx);

  return true;
}

bool ten_addon_manager_is_addon_loaded(ten_addon_manager_t *self,
                                       TEN_ADDON_TYPE addon_type,
                                       const char *addon_name) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(addon_name, "Invalid argument.");

  ten_rwlock_lock(self->rwlock, 1);

  ten_list_foreach (&self->registry, iter) {
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(iter.node);
    if (reg && reg->addon_type == addon_type &&
        ten_string_is_equal_c_str(&reg->addon_name, addon_name)) {
      ten_rwlock_unlock(self->rwlock, 1);
      return true;
    }
  }

  ten_rwlock_unlock(self->rwlock, 1);

  return false;
}

bool ten_addon_manager_set_belonging_app_if_not_set(ten_addon_manager_t *self,
                                                    ten_app_t *app) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_rwlock_lock(self->rwlock, 0);

  bool result = self->app == NULL;

  if (result) {
    self->app = app;
  }

  ten_rwlock_unlock(self->rwlock, 0);

  return result;
}

bool ten_addon_manager_belongs_to_app(ten_addon_manager_t *self,
                                      ten_app_t *app) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_rwlock_lock(self->rwlock, 1);

  bool result = self->app == app;

  ten_rwlock_unlock(self->rwlock, 1);

  return result;
}

ten_app_t *ten_addon_manager_get_belonging_app(ten_addon_manager_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  return self->app;
}

ten_addon_register_ctx_t *ten_addon_register_ctx_create(ten_app_t *app) {
  ten_addon_register_ctx_t *self = TEN_MALLOC(sizeof(ten_addon_register_ctx_t));
  TEN_ASSERT(self, "Failed to allocate memory.");
  TEN_ASSERT(app && ten_app_check_integrity(app, true), "Invalid argument.");

  self->app = app;

  return self;
}

void ten_addon_register_ctx_destroy(ten_addon_register_ctx_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_FREE(self);
}
