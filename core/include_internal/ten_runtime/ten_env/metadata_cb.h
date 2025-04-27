//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/ten_env/metadata.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/event.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_value_t ten_value_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct ten_extension_group_t ten_extension_group_t;

typedef void (*ten_env_peek_manifest_async_cb_t)(ten_env_t *ten_env,
                                                 ten_value_t *value,
                                                 void *cb_data,
                                                 ten_error_t *err);

typedef struct ten_peek_manifest_sync_context_t {
  ten_value_t *res;
  ten_event_t *completed;
} ten_peek_manifest_sync_context_t;

typedef struct ten_env_peek_property_async_context_t {
  ten_env_t *ten_env;
  ten_env_peek_property_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;

  union {
    ten_extension_t *extension;
    ten_extension_group_t *extension_group;
  } from;
} ten_env_peek_property_async_context_t;

typedef struct ten_env_peek_property_sync_context_t {
  ten_value_t *res;
  ten_event_t *completed;
} ten_env_peek_property_sync_context_t;

typedef struct ten_env_peek_manifest_async_context_t {
  ten_env_t *ten_env;
  ten_env_peek_manifest_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;

  union {
    ten_extension_t *extension;
    ten_extension_group_t *extension_group;
  } from;
} ten_env_peek_manifest_async_context_t;

typedef struct ten_env_peek_manifest_sync_context_t {
  ten_value_t *res;
  ten_event_t *completed;
} ten_env_peek_manifest_sync_context_t;

typedef struct ten_env_set_property_async_context_t {
  ten_env_t *ten_env;
  ten_env_set_property_async_cb_t cb;
  void *cb_data;

  // TODO(Liu): Replace with ten_error_t.
  bool res;

  union {
    ten_extension_t *extension;
    ten_extension_group_t *extension_group;
  } from;
} ten_env_set_property_async_context_t;

typedef struct ten_env_set_property_sync_context_t {
  ten_error_t *err;
  ten_event_t *completed;
} ten_env_set_property_sync_context_t;
