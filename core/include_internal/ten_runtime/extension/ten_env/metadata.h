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
typedef struct ten_extension_t ten_extension_t;

typedef void (*ten_extension_set_property_async_cb_t)(ten_extension_t *self,
                                                      bool res, void *cb_data,
                                                      ten_error_t *err);

typedef struct ten_extension_set_property_context_t {
  ten_string_t path;
  ten_value_t *value;
  ten_extension_set_property_async_cb_t cb;
  void *cb_data;
  bool res;
} ten_extension_set_property_context_t;

typedef void (*ten_extension_peek_property_async_cb_t)(ten_extension_t *self,
                                                       ten_value_t *value,
                                                       void *cb_data,
                                                       ten_error_t *err);

typedef struct ten_extension_peek_property_context_t {
  ten_string_t path;
  ten_extension_peek_property_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;
} ten_extension_peek_property_context_t;

typedef void (*ten_extension_peek_manifest_async_cb_t)(ten_extension_t *self,
                                                       ten_value_t *value,
                                                       void *cb_data,
                                                       ten_error_t *err);

typedef struct ten_extension_peek_manifest_context_t {
  ten_string_t path;
  ten_extension_peek_manifest_async_cb_t cb;
  void *cb_data;
  ten_value_t *res;
} ten_extension_peek_manifest_context_t;

TEN_RUNTIME_PRIVATE_API bool ten_extension_set_property(ten_extension_t *self,
                                                        const char *path,
                                                        ten_value_t *value,
                                                        ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_extension_set_property_async(
    ten_extension_t *self, const char *path, ten_value_t *value,
    ten_extension_set_property_async_cb_t cb, void *cb_data, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_extension_peek_property(
    ten_extension_t *extension, const char *path, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_extension_peek_property_async(
    ten_extension_t *self, const char *path,
    ten_extension_peek_property_async_cb_t cb, void *cb_data, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_extension_peek_manifest(
    ten_extension_t *self, const char *path, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_extension_peek_manifest_async(
    ten_extension_t *self, const char *path,
    ten_extension_peek_manifest_async_cb_t cb, void *cb_data, ten_error_t *err);
