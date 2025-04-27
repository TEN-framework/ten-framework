//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/ten_env/metadata.h"

#include "include_internal/ten_runtime/app/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

bool ten_extension_group_set_property(ten_extension_group_t *extension_group,
                                      const char *name, ten_value_t *value) {
  TEN_ASSERT(extension_group, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Invalid argument.");

  return ten_value_object_move(&extension_group->property, name, value);
}

static ten_extension_group_set_property_context_t *set_property_context_create(
    const char *name, ten_value_t *value,
    ten_extension_group_set_property_async_cb_t cb, void *cb_data) {
  ten_extension_group_set_property_context_t *set_prop =
      TEN_MALLOC(sizeof(ten_extension_group_set_property_context_t));
  TEN_ASSERT(set_prop, "Failed to allocate memory.");

  ten_string_init_formatted(&set_prop->name, name);
  set_prop->value = value;
  set_prop->cb = cb;
  set_prop->cb_data = cb_data;
  set_prop->res = false;

  return set_prop;
}

static void set_property_context_destroy(
    ten_extension_group_set_property_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->name);
  TEN_FREE(self);
}

static void ten_extension_group_set_property_task(void *self_, void *arg) {
  ten_extension_group_t *self = self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");

  ten_extension_group_set_property_context_t *set_property_context = arg;
  TEN_ASSERT(set_property_context, "Should not happen.");

  set_property_context->res = ten_extension_group_set_property(
      self, ten_string_get_raw_str(&set_property_context->name),
      set_property_context->value);

  if (set_property_context->cb) {
    set_property_context->cb(self, set_property_context->res,
                             set_property_context->cb_data);
  }

  set_property_context_destroy(set_property_context);
}

void ten_extension_group_set_property_async(
    ten_extension_group_t *self, const char *name, ten_value_t *value,
    ten_extension_group_set_property_async_cb_t cb, void *cb_data) {
  TEN_ASSERT(self, "Invalid argument.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function is intended to be called in any threads.
  TEN_ASSERT(ten_extension_group_check_integrity(self, false),
             "Invalid use of extension %p.", self);

  ten_extension_group_set_property_context_t *set_property_context =
      set_property_context_create(name, value, cb, cb_data);

  int rc = ten_runloop_post_task_tail(
      ten_extension_group_get_attached_runloop(self),
      ten_extension_group_set_property_task, self, set_property_context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

ten_value_t *ten_extension_group_peek_property(
    ten_extension_group_t *extension_group, const char *name) {
  TEN_ASSERT(extension_group, "Invalid argument.");
  TEN_ASSERT(ten_extension_group_check_integrity(extension_group, true),
             "Invalid argument.");

  ten_value_t *item = ten_value_object_peek(&extension_group->property, name);
  if (item == NULL) {
    return NULL;
  }

  return item;
}

static ten_extension_group_peek_property_context_t *
ten_extension_group_peek_property_context_create(
    const char *name, ten_extension_group_peek_property_async_cb_t cb,
    void *cb_data) {
  ten_extension_group_peek_property_context_t *context =
      TEN_MALLOC(sizeof(ten_extension_group_peek_property_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  ten_string_init_formatted(&context->name, name);
  context->cb = cb;
  context->cb_data = cb_data;
  context->res = NULL;

  return context;
}

static void ten_extension_group_peek_property_context_destroy(
    ten_extension_group_peek_property_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->name);

  TEN_FREE(self);
}

static void ten_extension_group_peek_property_task(void *self_, void *arg) {
  ten_extension_group_t *self = (ten_extension_group_t *)self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");

  ten_extension_group_peek_property_context_t *context =
      (ten_extension_group_peek_property_context_t *)arg;
  TEN_ASSERT(context, "Should not happen.");

  context->res = ten_extension_group_peek_property(
      self, ten_string_get_raw_str(&context->name));

  if (context->cb) {
    context->cb(self, context->res, context->cb_data);
  }

  ten_extension_group_peek_property_context_destroy(context);
}

void ten_extension_group_peek_property_async(
    ten_extension_group_t *self, const char *name,
    ten_extension_group_peek_property_async_cb_t cb, void *cb_data) {
  TEN_ASSERT(
      self && ten_extension_group_check_integrity(
                  self,
                  // TEN_NOLINTNEXTLINE(thread-check)
                  // thread-check: This function maybe called from any thread.
                  false),
      "Invalid argument.");

  ten_extension_group_peek_property_context_t *context =
      ten_extension_group_peek_property_context_create(name, cb, cb_data);

  int rc = ten_runloop_post_task_tail(
      ten_extension_group_get_attached_runloop(self),
      ten_extension_group_peek_property_task, self, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

ten_value_t *ten_extension_group_peek_manifest(ten_extension_group_t *self,
                                               const char *name) {
  TEN_ASSERT(
      self && ten_extension_group_check_integrity(
                  self,
                  // TEN_NOLINTNEXTLINE(thread-check)
                  // thread-check: This function maybe called from any thread.
                  false),
      "Invalid argument.");

  TEN_ASSERT(name, "Invalid argument.");

  ten_value_t *item = ten_value_object_peek(&self->manifest, name);
  if (item == NULL) {
    return NULL;
  }

  return item;
}

static ten_extension_group_peek_manifest_context_t *
ten_extension_group_peek_manifest_context_create(
    const char *name, ten_extension_group_peek_manifest_async_cb_t cb,
    void *cb_data) {
  ten_extension_group_peek_manifest_context_t *context =
      TEN_MALLOC(sizeof(ten_extension_group_peek_manifest_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  ten_string_init_formatted(&context->name, name);
  context->cb = cb;
  context->cb_data = cb_data;
  context->res = NULL;

  return context;
}

static void ten_extension_group_peek_manifest_context_destroy(
    ten_extension_group_peek_manifest_context_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_deinit(&self->name);

  TEN_FREE(self);
}

static void ten_extension_group_peek_manifest_task(void *self_, void *arg) {
  ten_extension_group_t *self = (ten_extension_group_t *)self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");

  ten_extension_group_peek_manifest_context_t *context =
      (ten_extension_group_peek_manifest_context_t *)arg;
  TEN_ASSERT(context, "Should not happen.");

  context->res = ten_extension_group_peek_manifest(
      self, ten_string_get_raw_str(&context->name));

  if (context->cb) {
    context->cb(self, context->res, context->cb_data);
  }

  ten_extension_group_peek_manifest_context_destroy(context);
}

void ten_extension_group_peek_manifest_async(
    ten_extension_group_t *self, const char *name,
    ten_extension_group_peek_manifest_async_cb_t cb, void *cb_data) {
  TEN_ASSERT(
      self && ten_extension_group_check_integrity(
                  self,
                  // TEN_NOLINTNEXTLINE(thread-check)
                  // thread-check: This function maybe called from any thread.
                  false),
      "Invalid argument.");

  ten_extension_group_peek_manifest_context_t *context =
      ten_extension_group_peek_manifest_context_create(name, cb, cb_data);

  int rc = ten_runloop_post_task_tail(
      ten_extension_group_get_attached_runloop(self),
      ten_extension_group_peek_manifest_task, self, context);
  if (rc) {
    TEN_LOGW("Failed to post task to extension group's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}
