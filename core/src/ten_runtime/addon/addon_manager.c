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

    instance->mutex = ten_mutex_create();
    TEN_ASSERT(instance->mutex, "Failed to create addon manager mutex.");

    instance->app = NULL;
  }

  ten_mutex_unlock(init_mutex);

  return instance;
}

void ten_addon_manager_destroy(ten_addon_manager_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_mutex_destroy(self->mutex);
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
                                 void *user_data, ten_error_t *error) {
  TEN_ASSERT(self && addon_name && func, "Invalid argument.");

  TEN_ADDON_TYPE addon_type = ten_addon_type_from_string(addon_type_str);
  if (addon_type == TEN_ADDON_TYPE_INVALID) {
    TEN_LOGF("Invalid addon type: %s", addon_type_str);
    if (error) {
      ten_error_set(error, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Invalid addon type: %s", addon_type_str);
    }
    return false;
  }

  ten_mutex_lock(self->mutex);

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
    reg->user_data = user_data;

    // Add to the registry.
    ten_list_push_ptr_back(&self->registry, reg,
                           ten_addon_registration_destroy);
  } else {
    // Handle the case where the addon is already added.
    // For now, log a warning.
    TEN_LOGW("Addon '%s:%s' is already registered.", addon_type_str,
             addon_name);
  }

  ten_mutex_unlock(self->mutex);

  return true;
}

void ten_addon_manager_register_all_addons(ten_addon_manager_t *self,
                                           void *register_ctx) {
  TEN_ASSERT(self, "Invalid argument.");

  // Basically, the relationship between an app and a process is one-to-one,
  // meaning a process will only have one app. In this scenario, theoretically,
  // the mutex lock/unlock protection below would not be necessary. However, in
  // special cases, such as under gtest, a single gtest process may execute
  // multiple apps, with some apps starting in parallel. As a result, this
  // function could potentially be called multiple times in parallel. In such
  // cases, the mutex lock/unlock is indeed necessary. In normal circumstances,
  // where the relationship is one-to-one, performing the mutex lock/unlock
  // action causes no harm. Therefore, mutex lock/unlock is uniformly applied
  // here.
  ten_mutex_lock(self->mutex);

  ten_list_iterator_t iter = ten_list_begin(&self->registry);
  while (!ten_list_iterator_is_end(iter)) {
    ten_listnode_t *node = iter.node;
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(node);

    if (reg && reg->func) {
      reg->func(reg->addon_type, &reg->addon_name, register_ctx,
                reg->user_data);
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_mutex_unlock(self->mutex);
}

void ten_addon_manager_register_all_addon_loaders(ten_addon_manager_t *self,
                                                  void *register_ctx) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Invalid argument.");

  ten_mutex_lock(self->mutex);

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
        reg->func(reg->addon_type, &reg->addon_name, register_ctx,
                  reg->user_data);
      }
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_mutex_unlock(self->mutex);
}

void ten_addon_manager_register_all_protocols(ten_addon_manager_t *self,
                                              void *register_ctx) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self->app, true), "Invalid argument.");

  ten_mutex_lock(self->mutex);

  ten_list_iterator_t iter = ten_list_begin(&self->registry);
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
        reg->func(reg->addon_type, &reg->addon_name, register_ctx,
                  reg->user_data);
      }
    }

    iter = ten_list_iterator_next(iter);
  }

  ten_mutex_unlock(self->mutex);
}

bool ten_addon_manager_register_specific_addon(ten_addon_manager_t *self,
                                               TEN_ADDON_TYPE addon_type,
                                               const char *addon_name,
                                               void *register_ctx) {
  TEN_ASSERT(self && addon_name, "Invalid argument.");

  bool success = false;

  ten_mutex_lock(self->mutex);

  // Iterate through the registry to find the specific addon.
  ten_list_foreach (&self->registry, iter) {
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(iter.node);
    if (reg && reg->addon_type == addon_type &&
        ten_string_is_equal_c_str(&reg->addon_name, addon_name)) {
      reg->func(addon_type, &reg->addon_name, register_ctx, reg->user_data);
      success = true;
      break;
    }
  }

  if (!success) {
    TEN_LOGI("Unable to find '%s:%s' in registry.",
             ten_addon_type_to_string(addon_type), addon_name);
  }

  ten_mutex_unlock(self->mutex);

  return success;
}

bool ten_addon_manager_is_addon_loaded(ten_addon_manager_t *self,
                                       TEN_ADDON_TYPE addon_type,
                                       const char *addon_name) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(addon_name, "Invalid argument.");

  ten_mutex_lock(self->mutex);

  ten_list_foreach (&self->registry, iter) {
    ten_addon_registration_t *reg =
        (ten_addon_registration_t *)ten_ptr_listnode_get(iter.node);
    if (reg && reg->addon_type == addon_type &&
        ten_string_is_equal_c_str(&reg->addon_name, addon_name)) {
      ten_mutex_unlock(self->mutex);
      return true;
    }
  }

  ten_mutex_unlock(self->mutex);

  return false;
}

bool ten_addon_manager_set_belonging_app_if_not_set(ten_addon_manager_t *self,
                                                    ten_app_t *app) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_mutex_lock(self->mutex);

  bool result = self->app == NULL;

  if (result) {
    self->app = app;
  }

  ten_mutex_unlock(self->mutex);

  return result;
}

bool ten_addon_manager_belongs_to_app(ten_addon_manager_t *self,
                                      ten_app_t *app) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_mutex_lock(self->mutex);

  bool result = self->app == app;

  ten_mutex_unlock(self->mutex);

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
