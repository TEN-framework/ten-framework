//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/extension_group.h"

#include <stdlib.h>
#include <time.h>

#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/value/value.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/msg/cmd_result/cmd_result.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/sanitizer/thread_check.h"
#include "ten_utils/value/value.h"

bool ten_extension_group_check_integrity(ten_extension_group_t *self,
                                         bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_EXTENSION_GROUP_SIGNATURE) {
    return false;
  }
  if (self->binding_handle.me_in_target_lang == NULL) {
    return false;
  }

  if (check_thread) {
    // Note that the 'extension_thread' might be NULL when the extension group
    // is newly created.
    ten_extension_thread_t *extension_thread = self->extension_thread;
    if (extension_thread) {
      return ten_extension_thread_check_integrity(extension_thread, true);
    }

    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

ten_extension_group_t *ten_extension_group_create_internal(
    const char *name, ten_extension_group_on_configure_func_t on_configure,
    ten_extension_group_on_init_func_t on_init,
    ten_extension_group_on_deinit_func_t on_deinit,
    ten_extension_group_on_create_extensions_func_t on_create_extensions,
    ten_extension_group_on_destroy_extensions_func_t on_destroy_extensions) {
  ten_extension_group_t *self =
      (ten_extension_group_t *)TEN_MALLOC(sizeof(ten_extension_group_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_EXTENSION_GROUP_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->addon_host = NULL;
  if (name) {
    ten_string_init_formatted(&self->name, "%s", name);
  }

  self->on_configure = on_configure;
  self->on_init = on_init;
  self->on_deinit = on_deinit;
  self->on_create_extensions = on_create_extensions;
  self->on_destroy_extensions = on_destroy_extensions;

  // This variable might be replaced later by the target language world.
  self->binding_handle.me_in_target_lang = self;

  self->extension_group_info = NULL;
  self->extension_thread = NULL;
  self->ten_env = NULL;

  ten_list_init(&self->extension_addon_and_instance_name_pairs);
  ten_error_init(&self->err_before_ready);

  ten_value_init_object_with_move(&self->manifest, NULL);
  ten_value_init_object_with_move(&self->property, NULL);

  self->manifest_info = NULL;
  self->property_info = NULL;

  self->app = NULL;
  self->extension_context = NULL;
  self->state = TEN_EXTENSION_GROUP_STATE_INIT;
  self->extensions_cnt_of_being_destroyed = 0;

  return self;
}

ten_extension_group_t *ten_extension_group_create(
    const char *name, ten_extension_group_on_configure_func_t on_configure,
    ten_extension_group_on_init_func_t on_init,
    ten_extension_group_on_deinit_func_t on_deinit,
    ten_extension_group_on_create_extensions_func_t on_create_extensions,
    ten_extension_group_on_destroy_extensions_func_t on_destroy_extensions) {
  TEN_ASSERT(name, "Should not happen.");
  TEN_ASSERT(on_create_extensions, "Should not happen.");
  TEN_ASSERT(on_destroy_extensions, "Should not happen.");

  ten_extension_group_t *self = ten_extension_group_create_internal(
      name, on_configure, on_init, on_deinit, on_create_extensions,
      on_destroy_extensions);
  self->ten_env = ten_env_create_for_extension_group(self);

  return self;
}

void ten_extension_group_destroy(ten_extension_group_t *self) {
  // TODO(Wei): It's strange that JS main thread would call this function, need
  // further investigation.
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(self->extensions_cnt_of_being_destroyed == 0,
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  if (self->ten_env) {
    ten_env_destroy(self->ten_env);
  }

  ten_error_deinit(&self->err_before_ready);
  ten_list_clear(&self->extension_addon_and_instance_name_pairs);

  ten_value_deinit(&self->manifest);
  ten_value_deinit(&self->property);

  if (self->manifest_info) {
    ten_metadata_info_destroy(self->manifest_info);
    self->manifest_info = NULL;
  }

  if (self->property_info) {
    ten_metadata_info_destroy(self->property_info);
    self->property_info = NULL;
  }

  ten_string_deinit(&self->name);

  if (self->addon_host) {
    // Since the extension has already been destroyed, there is no need to
    // release its resources through the corresponding addon anymore. Therefore,
    // decrement the reference count of the corresponding addon.
    ten_ref_dec_ref(&self->addon_host->ref);
    self->addon_host = NULL;
  }

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

void ten_extension_group_create_extensions(ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(self->on_create_extensions, "Should not happen.");
  TEN_ASSERT(self->ten_env && ten_env_check_integrity(self->ten_env, true),
             "Should not happen.");

  TEN_LOGD("[%s] create_extensions.", ten_extension_group_get_name(self, true));

  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread, "Should not happen.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_thread_set_state(
      extension_thread, TEN_EXTENSION_THREAD_STATE_CREATING_EXTENSIONS);

  self->on_create_extensions(self, self->ten_env);
}

void ten_extension_group_destroy_extensions(ten_extension_group_t *self,
                                            ten_list_t extensions) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(self->on_destroy_extensions, "Should not happen.");
  TEN_ASSERT(self->ten_env && ten_env_check_integrity(self->ten_env, true),
             "Should not happen.");

  TEN_LOGD("[%s] destroy_extensions.",
           ten_extension_group_get_name(self, true));

  self->on_destroy_extensions(self, self->ten_env, extensions);
}

void ten_extension_group_set_addon(ten_extension_group_t *self,
                                   ten_addon_host_t *addon_host) {
  TEN_ASSERT(self, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: in the case of JS binding, the extension group would
  // initially created in the JS main thread, and and engine thread will
  // call this function. However, these operations are all occurred
  // before the whole extension system is running, so it's thread safe.
  TEN_ASSERT(ten_extension_group_check_integrity(self, false),
             "Should not happen.");

  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host), "Should not happen.");

  // Since the extension requires the corresponding addon to release
  // its resources, therefore, hold on to a reference count of the corresponding
  // addon.
  TEN_ASSERT(!self->addon_host, "Should not happen.");
  self->addon_host = addon_host;
  ten_ref_inc_ref(&addon_host->ref);
}

ten_shared_ptr_t *ten_extension_group_create_cmd_result_for_invalid_dest(
    ten_shared_ptr_t *origin_cmd, ten_string_t *target_group_name) {
  TEN_ASSERT(origin_cmd, "Should not happen.");
  TEN_ASSERT(target_group_name, "Should not happen.");

  if (!ten_msg_is_cmd_and_result(origin_cmd)) {
    ten_msg_dump(origin_cmd, NULL, "Unexpected message: ^m");
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_shared_ptr_t *cmd_result =
      ten_cmd_result_create_from_cmd(TEN_STATUS_CODE_ERROR, origin_cmd);
  ten_msg_set_property(
      cmd_result, TEN_STR_DETAIL,
      ten_value_create_vstring("The extension group[%s] is invalid.",
                               ten_string_get_raw_str(target_group_name)),
      NULL);

  return cmd_result;
}

ten_runloop_t *ten_extension_group_get_attached_runloop(
    ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // other threads.
      ten_extension_group_check_integrity(self, false), "Should not happen.");

  return self->extension_thread->runloop;
}

ten_list_t *ten_extension_group_get_extension_addon_and_instance_name_pairs(
    ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // other threads.
      ten_extension_group_check_integrity(self, false), "Should not happen.");

  return &self->extension_addon_and_instance_name_pairs;
}

ten_env_t *ten_extension_group_get_ten_env(ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // other threads.
      ten_extension_group_check_integrity(self, false), "Should not happen.");

  return self->ten_env;
}

void ten_extension_group_set_extension_cnt_of_being_destroyed(
    ten_extension_group_t *self, size_t new_cnt) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // other threads.
      ten_extension_group_check_integrity(self, false), "Should not happen.");

  self->extensions_cnt_of_being_destroyed = new_cnt;
}

size_t ten_extension_group_decrement_extension_cnt_of_being_destroyed(
    ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in
      // other threads.
      ten_extension_group_check_integrity(self, false), "Should not happen.");

  return --self->extensions_cnt_of_being_destroyed;
}

const char *ten_extension_group_get_name(ten_extension_group_t *self,
                                         bool check_thread) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, check_thread),
             "Invalid use of extension group %p.", self);

  return ten_string_get_raw_str(&self->name);
}
