//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/binding/common.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/macro/check.h"

#define TEN_ENV_SIGNATURE 0x1336D348DA779EA6U

typedef struct ten_engine_t ten_engine_t;
typedef struct ten_addon_loader_t ten_addon_loader_t;

typedef void (*ten_env_destroy_handler_in_target_lang_func_t)(
    void *me_in_target_lang);

typedef enum TEN_ENV_ATTACH_TO {
  TEN_ENV_ATTACH_TO_INVALID,

  TEN_ENV_ATTACH_TO_EXTENSION,
  TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
  TEN_ENV_ATTACH_TO_APP,
  TEN_ENV_ATTACH_TO_ENGINE,
  TEN_ENV_ATTACH_TO_ADDON,
  TEN_ENV_ATTACH_TO_ADDON_LOADER,
} TEN_ENV_ATTACH_TO;

typedef struct ten_env_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_ENV_ATTACH_TO attach_to;

  union {
    // This is the extension which this ten_env_t object applies.
    ten_extension_t *extension;

    // This is the extension group which this ten_env_t object applies.
    ten_extension_group_t *extension_group;

    // This is the app which this ten_env_t object applies.
    ten_app_t *app;

    // This is the addon which this ten_env_t object applies.
    ten_addon_host_t *addon_host;

    // This is the engine which this ten_env_t object applies.
    ten_engine_t *engine;

    // This is the addon loader which this ten_env_t object applies.
    ten_addon_loader_t *addon_loader;
  } attached_target;

  // This flag indicates whether ten_env has been closed. If closed, no ten_env
  // methods can be called (except ten_env_log). So it can also be viewed the
  // other way around: when all APIs of `ten_env` (except for `ten_env_log`) can
  // no longer be called, the `is_closed` flag can be set.
  //
  // This flag is only set to true in `ten_env_close()`.
  // For app/extension/extension_group/addon, `ten_env_close()` is called in
  // ten_env_on_deinit_done().
  // For engine, `ten_env_close()` is called when the engine is closed.
  bool is_closed;

  // This handler is called when the ten_env is willing to be destroyed.
  ten_env_destroy_handler_in_target_lang_func_t destroy_handler;

  ten_list_t ten_proxy_list;
} ten_env_t;

TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_env_get_attached_runloop(
    ten_env_t *self);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_extension_group(
    ten_extension_group_t *extension_group);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_addon(
    ten_addon_host_t *addon_host);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_extension(
    ten_extension_t *extension);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_app(ten_app_t *app);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_engine(
    ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create_for_addon_loader(
    ten_addon_loader_t *addon_loader);

TEN_RUNTIME_PRIVATE_API void ten_env_close(ten_env_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_env_is_closed(ten_env_t *self);

TEN_RUNTIME_PRIVATE_API ten_env_t *ten_env_create(void);

TEN_RUNTIME_API void ten_env_set_destroy_handler_in_target_lang(
    ten_env_t *self, ten_env_destroy_handler_in_target_lang_func_t handler);

TEN_RUNTIME_API TEN_ENV_ATTACH_TO ten_env_get_attach_to(ten_env_t *self);

TEN_RUNTIME_PRIVATE_API void ten_env_set_attach_to(
    ten_env_t *self, TEN_ENV_ATTACH_TO attach_to_type, void *attach_to);

TEN_RUNTIME_PRIVATE_API const char *ten_env_get_attached_instance_name(
    ten_env_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_app_t *ten_env_get_belonging_app(ten_env_t *self);

inline ten_extension_t *ten_env_get_attached_extension(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION,
             "Should not happen.");

  return self->attached_target.extension;
}

inline ten_extension_group_t *ten_env_get_attached_extension_group(
    ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");

  return self->attached_target.extension_group;
}

inline ten_app_t *ten_env_get_attached_app(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");

  return self->attached_target.app;
}

inline ten_addon_host_t *ten_env_get_attached_addon(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");

  return self->attached_target.addon_host;
}

inline ten_engine_t *ten_env_get_attached_engine(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ENGINE, "Should not happen.");

  return self->attached_target.engine;
}

inline ten_addon_loader_t *ten_env_get_attached_addon_loader(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: self->attach_to is not changed after ten is created.
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);

  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON_LOADER,
             "Should not happen.");

  return self->attached_target.addon_loader;
}
