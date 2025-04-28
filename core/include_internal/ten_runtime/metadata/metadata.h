//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/schema_store/store.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/value/value.h"

typedef struct ten_metadata_info_t ten_metadata_info_t;
typedef struct ten_env_t ten_env_t;

typedef void (*ten_object_on_configure_func_t)(ten_env_t *ten_env);

typedef enum TEN_METADATA_TYPE {
  TEN_METADATA_INVALID,

  TEN_METADATA_JSON_FILENAME,
  TEN_METADATA_JSON_STR,

  TEN_METADATA_LAST,
} TEN_METADATA_TYPE;

TEN_RUNTIME_API bool ten_metadata_info_set(ten_metadata_info_t *self,
                                           TEN_METADATA_TYPE type,
                                           const char *value);

TEN_RUNTIME_API void ten_metadata_info_destroy(ten_metadata_info_t *self);

TEN_RUNTIME_PRIVATE_API void ten_metadata_load(
    ten_object_on_configure_func_t on_configure, ten_env_t *ten_env);

TEN_RUNTIME_PRIVATE_API bool ten_metadata_load_from_info(
    ten_value_t *metadata, ten_metadata_info_t *metadata_info,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_metadata_init_schema_store(
    ten_value_t *manifest, ten_schema_store_t *schema_store);

TEN_RUNTIME_PRIVATE_API bool ten_manifest_json_string_is_valid(
    const char *json_str, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_manifest_json_file_is_valid(
    const char *json_file, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_property_json_string_is_valid(
    const char *json_str, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_property_json_file_is_valid(
    const char *json_file, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_handle_manifest_info_when_on_configure_done(
    ten_metadata_info_t **self, const char *base_dir, ten_value_t *manifest,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_handle_property_info_when_on_configure_done(
    ten_metadata_info_t **self, const char *base_dir, ten_value_t *property,
    ten_error_t *err);
