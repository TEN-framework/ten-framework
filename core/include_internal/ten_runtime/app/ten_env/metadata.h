//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"

typedef struct ten_env_t ten_env_t;
typedef struct ten_value_t ten_value_t;
typedef struct ten_app_t ten_app_t;

typedef void (*ten_app_set_property_async_cb_t)(ten_app_t *self,
                                                ten_error_t *err,
                                                void *cb_data);

typedef struct ten_app_set_property_context_t {
  ten_string_t name;
  ten_value_t *value;
  ten_app_set_property_async_cb_t cb;
  void *cb_data;
  ten_error_t err;
} ten_app_set_property_context_t;

typedef void (*ten_app_peek_property_async_cb_t)(ten_app_t *self,
                                                 ten_value_t *value,
                                                 void *cb_data);

typedef struct ten_app_peek_property_context_t {
  ten_string_t name;
  ten_app_peek_property_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;
} ten_app_peek_property_context_t;

typedef void (*ten_app_peek_manifest_async_cb_t)(ten_app_t *self,
                                                 ten_value_t *value,
                                                 void *cb_data);

typedef struct ten_app_peek_manifest_context_t {
  ten_string_t name;
  ten_app_peek_manifest_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;
} ten_app_peek_manifest_context_t;

TEN_RUNTIME_PRIVATE_API bool ten_app_set_property(ten_app_t *self,
                                                  const char *name,
                                                  ten_value_t *value,
                                                  ten_error_t *err);

TEN_RUNTIME_PRIVATE_API void ten_app_set_property_async(
    ten_app_t *self, const char *name, ten_value_t *value,
    ten_app_set_property_async_cb_t cb, void *cb_data);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_app_peek_property(ten_app_t *app,
                                                           const char *name);

TEN_RUNTIME_PRIVATE_API void ten_app_peek_property_async(
    ten_app_t *self, const char *name, ten_app_peek_property_async_cb_t cb,
    void *cb_data);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_app_peek_manifest(ten_app_t *self,
                                                           const char *name);

TEN_RUNTIME_PRIVATE_API void ten_app_peek_manifest_async(
    ten_app_t *self, const char *name, ten_app_peek_manifest_async_cb_t cb,
    void *cb_data);
