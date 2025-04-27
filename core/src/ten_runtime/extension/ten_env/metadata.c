//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/ten_env/metadata.h"

#include "include_internal/ten_runtime/app/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/metadata.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "include_internal/ten_runtime/ten_env/metadata_cb.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/value/value.h"

bool ten_extension_set_property(ten_extension_t *self, const char *name,
                                ten_value_t *value, ten_error_t *err) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Invalid argument.");

  if (!ten_schema_store_adjust_property_kv(&self->schema_store, name, value,
                                           err)) {
    return false;
  }

  if (!ten_schema_store_validate_property_kv(&self->schema_store, name, value,
                                             err)) {
    return false;
  }

  return ten_value_set_from_path_str_with_move(&self->property, name, value,
                                               err);
}

static ten_extension_set_property_context_t *set_property_context_create(
    const char *name, ten_value_t *value,
    ten_extension_set_property_async_cb_t cb, void *cb_data) {
  ten_extension_set_property_context_t *set_prop =
      TEN_MALLOC(sizeof(ten_extension_set_property_context_t));
  TEN_ASSERT(set_prop, "Failed to allocate memory.");

  ten_string_init_formatted(&set_prop->path, name);
  set_prop->value = value;
  set_prop->cb = cb;
  set_prop->cb_data = cb_data;
  set_prop->res = false;

  return set_prop;
}

static void set_property_context_destroy(
    ten_extension_set_property_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->path);
  TEN_FREE(self);
}

static void ten_extension_set_property_task(void *self_, void *arg) {
  ten_extension_t *self = self_;
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_extension_thread_t *extension_thread = self->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  ten_extension_set_property_context_t *set_property_context = arg;
  TEN_ASSERT(set_property_context, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  set_property_context->res = ten_extension_set_property(
      self, ten_string_get_raw_str(&set_property_context->path),
      set_property_context->value, &err);

  if (set_property_context->cb) {
    set_property_context->cb(self, set_property_context->res,
                             set_property_context->cb_data, &err);
  }

  set_property_context_destroy(set_property_context);

  ten_error_deinit(&err);
}

bool ten_extension_set_property_async(ten_extension_t *self, const char *path,
                                      ten_value_t *value,
                                      ten_extension_set_property_async_cb_t cb,
                                      void *cb_data,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_extension_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  ten_extension_set_property_context_t *set_property_context =
      set_property_context_create(path, value, cb, cb_data);

  int rc = ten_runloop_post_task_tail(ten_extension_get_attached_runloop(self),
                                      ten_extension_set_property_task, self,
                                      set_property_context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  return true;
}

ten_value_t *ten_extension_peek_property(ten_extension_t *extension,
                                         const char *path, ten_error_t *err) {
  TEN_ASSERT(extension && ten_extension_check_integrity(extension, true),
             "Invalid argument.");

  if (!path || !strlen(path)) {
    // If the path is empty, return all properties.
    return &extension->property;
  }

  ten_extension_thread_t *extension_thread = extension->extension_thread;
  TEN_ASSERT(extension_thread, "Invalid argument.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Invalid use of extension_thread %p.", extension_thread);

  ten_value_t *v = ten_value_peek_from_path(&extension->property, path, err);
  if (!v) {
    return NULL;
  }

  return v;
}

static ten_extension_peek_property_context_t *
ten_extension_peek_property_context_create(
    const char *name, ten_extension_peek_property_async_cb_t cb,
    void *cb_data) {
  ten_extension_peek_property_context_t *context =
      TEN_MALLOC(sizeof(ten_extension_peek_property_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  ten_string_init_formatted(&context->path, name);
  context->cb = cb;
  context->cb_data = cb_data;
  context->res = NULL;

  return context;
}

static void ten_extension_peek_property_context_destroy(
    ten_extension_peek_property_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->path);

  TEN_FREE(self);
}

static void ten_extension_peek_property_task(void *self_, void *arg) {
  ten_extension_t *self = (ten_extension_t *)self_;
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_extension_peek_property_context_t *context =
      (ten_extension_peek_property_context_t *)arg;
  TEN_ASSERT(context, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  context->res = ten_extension_peek_property(
      self, ten_string_get_raw_str(&context->path), &err);

  if (context->cb) {
    context->cb(self, context->res, context->cb_data, &err);
  }

  ten_extension_peek_property_context_destroy(context);

  ten_error_deinit(&err);
}

bool ten_extension_peek_property_async(
    ten_extension_t *self, const char *path,
    ten_extension_peek_property_async_cb_t cb, void *cb_data,
    TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(
      self && ten_extension_check_integrity(
                  self,
                  // TEN_NOLINTNEXTLINE(thread-check)
                  // thread-check: This function maybe called from any thread.
                  false),
      "Invalid argument.");

  ten_extension_peek_property_context_t *context =
      ten_extension_peek_property_context_create(path, cb, cb_data);

  int rc = ten_runloop_post_task_tail(self->extension_thread->runloop,
                                      ten_extension_peek_property_task, self,
                                      context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  return true;
}

ten_value_t *ten_extension_peek_manifest(ten_extension_t *self,
                                         const char *path, ten_error_t *err) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The manifest of an extension is read-only, so it's safe to
  // access it from any threads.
  TEN_ASSERT(self && ten_extension_check_integrity(self, false),
             "Invalid argument.");
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || !strlen(path)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "path should not be empty.");
    }
    return NULL;
  }

  ten_value_t *v = ten_value_peek_from_path(&self->manifest, path, err);
  if (!v) {
    return NULL;
  }

  return v;
}

static ten_extension_peek_manifest_context_t *
ten_extension_peek_manifest_context_create(
    const char *name, ten_extension_peek_manifest_async_cb_t cb,
    void *cb_data) {
  ten_extension_peek_manifest_context_t *context =
      TEN_MALLOC(sizeof(ten_extension_peek_manifest_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  ten_string_init_formatted(&context->path, name);
  context->cb = cb;
  context->cb_data = cb_data;
  context->res = NULL;

  return context;
}

static void ten_extension_peek_manifest_context_destroy(
    ten_extension_peek_manifest_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->path);

  TEN_FREE(self);
}

static void ten_extension_peek_manifest_task(void *self_, void *arg) {
  ten_extension_t *self = (ten_extension_t *)self_;
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_extension_peek_manifest_context_t *context =
      (ten_extension_peek_manifest_context_t *)arg;
  TEN_ASSERT(context, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  context->res = ten_extension_peek_manifest(
      self, ten_string_get_raw_str(&context->path), &err);

  if (context->cb) {
    context->cb(self, context->res, context->cb_data, &err);
  }

  ten_extension_peek_manifest_context_destroy(context);

  ten_error_deinit(&err);
}

bool ten_extension_peek_manifest_async(
    ten_extension_t *self, const char *path,
    ten_extension_peek_manifest_async_cb_t cb, void *cb_data,
    TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(
      self && ten_extension_check_integrity(
                  self,
                  // TEN_NOLINTNEXTLINE(thread-check)
                  // thread-check: This function maybe called from any thread.
                  false),
      "Invalid argument.");

  ten_extension_peek_manifest_context_t *context =
      ten_extension_peek_manifest_context_create(path, cb, cb_data);

  int rc = ten_runloop_post_task_tail(self->extension_thread->runloop,
                                      ten_extension_peek_manifest_task, self,
                                      context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  return true;
}
