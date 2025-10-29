//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/metadata/metadata.h"

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/file.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_object.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_rust/ten_rust.h"
#endif

static bool ten_metadata_load_from_json_str(ten_value_t *metadata,
                                            const char *json_str,
                                            ten_error_t *err) {
  TEN_ASSERT(metadata && ten_value_check_integrity(metadata) && json_str,
             "Should not happen.");

  ten_json_t *json = ten_json_from_string(json_str, err);
  if (!json) {
    return false;
  }

  bool rc = ten_value_object_merge_with_json(metadata, json);

  ten_json_destroy(json);

  return rc;
}

static bool ten_metadata_load_from_json_file(ten_value_t *metadata,
                                             const char *filename,
                                             ten_error_t *err) {
  TEN_ASSERT(metadata && ten_value_check_integrity(metadata),
             "Should not happen.");

  if (!filename || strlen(filename) == 0) {
    TEN_LOGW("Try to load metadata but file name not provided");
    return false;
  }

  char *buf = ten_file_read(filename);
  if (!buf) {
    TEN_LOGW("Can not read content from %s", filename);
    return false;
  }

  bool ret = ten_metadata_load_from_json_str(metadata, buf, err);
  if (!ret) {
    TEN_LOGW(
        "Try to load metadata from file '%s', but file content with wrong "
        "format",
        filename);
  }

  TEN_FREE(buf);

  return ret;
}

static bool ten_metadata_load_from_type_ane_value(ten_value_t *metadata,
                                                  TEN_METADATA_TYPE type,
                                                  const char *value,
                                                  ten_error_t *err) {
  TEN_ASSERT(metadata, "Invalid argument.");

  bool result = true;

  switch (type) {
  case TEN_METADATA_INVALID:
    break;

  case TEN_METADATA_JSON_STR:
    if (!ten_metadata_load_from_json_str(metadata, value, err)) {
      result = false;
      goto done;
    }
    break;

  case TEN_METADATA_JSON_FILENAME:
    if (!ten_metadata_load_from_json_file(metadata, value, err)) {
      result = false;
      goto done;
    }
    break;

  default:
    TEN_ASSERT(0 && "Handle more types.", "Should not happen.");
    break;
  }

done:
  return result;
}

bool ten_metadata_load_from_info(ten_value_t *metadata,
                                 ten_metadata_info_t *metadata_info,
                                 ten_error_t *err) {
  TEN_ASSERT(metadata && metadata_info, "Invalid argument.");

  return ten_metadata_load_from_type_ane_value(
      metadata, metadata_info->type,
      metadata_info->value ? ten_string_get_raw_str(metadata_info->value) : "",
      err);
}

void ten_metadata_load(ten_object_on_configure_func_t on_configure,
                       ten_env_t *ten_env) {
  TEN_ASSERT(on_configure && ten_env && ten_env_check_integrity(ten_env, true),
             "Should not happen.");

  on_configure(ten_env);
}

// This function is used to flatten the manifest api definition.
// If the manifest api definition is invalid, the return value is NULL.
// Note that if the return value is not NULL, it means the return value is a new
// value, and the caller should destroy it after using.
static ten_value_t *ten_metadata_flatten_manifest_api_definition(
    ten_value_t *api_definition, const char *base_dir,
    const char *app_base_dir) {
  TEN_ASSERT(api_definition, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(api_definition), "Invalid argument.");
  TEN_ASSERT(base_dir, "Invalid argument.");
  TEN_ASSERT(app_base_dir, "Invalid argument.");

  ten_value_t *result = NULL;

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  ten_json_t original_api_json =
      TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(api_definition, &original_api_json);
  if (!success) {
    TEN_LOGW("Failed to jsonify api definition.");
    ten_json_deinit(&original_api_json);
    return NULL;
  }

  bool must_free_original_json_str = false;
  const char *original_api_json_str = ten_json_to_string(
      &original_api_json, NULL, &must_free_original_json_str);
  TEN_ASSERT(original_api_json_str, "Should not happen.");

  char *err_msg = NULL;
  const char *flattened_api_json_str = ten_rust_manifest_api_flatten(
      original_api_json_str, base_dir, app_base_dir, &err_msg);

  if (must_free_original_json_str) {
    TEN_FREE(original_api_json_str);
  }

  ten_json_deinit(&original_api_json);

  if (!flattened_api_json_str) {
    TEN_LOGW("Failed to flatten manifest api definition: %s", err_msg);
    ten_rust_free_cstring(err_msg);
    return NULL;
  }

  ten_value_t *flattened_api_definition =
      ten_value_from_json_str(flattened_api_json_str);
  if (!flattened_api_definition) {
    TEN_LOGE(
        "Failed to create value from flattened api definition json string.");
    ten_rust_free_cstring(flattened_api_json_str);
    return NULL;
  }

  ten_rust_free_cstring(flattened_api_json_str);

  result = flattened_api_definition;
#endif

  return result;
}

bool ten_metadata_init_schema_store(ten_value_t *manifest,
                                    ten_schema_store_t *schema_store,
                                    const char *base_dir,
                                    const char *app_base_dir) {
  TEN_ASSERT(manifest, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(manifest), "Invalid argument.");
  TEN_ASSERT(ten_value_is_object(manifest), "Should not happen.");
  TEN_ASSERT(schema_store, "Invalid argument.");

  ten_value_t *api_definition = ten_value_object_peek(manifest, TEN_STR_API);
  if (!api_definition) {
    return false;
  }

  bool value_need_free = false;

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  api_definition = ten_metadata_flatten_manifest_api_definition(
      api_definition, base_dir, app_base_dir);
  if (!api_definition) {
    TEN_LOGE(
        "Failed to flatten manifest api definition. base_dir: %s, "
        "app_base_dir: %s",
        base_dir, app_base_dir);
    return false;
  }

  value_need_free = true;
#endif

  ten_error_t err;
  TEN_ERROR_INIT(err);
  if (!ten_schema_store_set_schema_definition(schema_store, api_definition,
                                              &err)) {
    TEN_LOGW("Failed to set schema definition: %s", ten_error_message(&err));
  }

  ten_error_deinit(&err);

  if (value_need_free) {
    ten_value_destroy(api_definition);
  }

  return true;
}

bool ten_manifest_json_string_is_valid(const char *json_string,
                                       ten_error_t *err) {
  TEN_ASSERT(json_string, "Invalid argument.");
  TEN_ASSERT(err, "Invalid argument.");
  TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  const char *err_msg = NULL;
  bool rc = ten_validate_manifest_json_string(json_string, &err_msg);

  if (!rc) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
    ten_rust_free_cstring(err_msg);
    return false;
  }
#endif

  return true;
}

bool ten_manifest_json_file_is_valid(const char *json_file, ten_error_t *err) {
  TEN_ASSERT(json_file, "Invalid argument.");
  TEN_ASSERT(err, "Invalid argument.");
  TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  const char *err_msg = NULL;
  bool rc = ten_validate_manifest_json_file(json_file, &err_msg);

  if (!rc) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
    ten_rust_free_cstring(err_msg);
    return false;
  }
#endif

  return true;
}

bool ten_property_json_string_is_valid(const char *json_string,
                                       ten_error_t *err) {
  TEN_ASSERT(json_string, "Invalid argument.");
  TEN_ASSERT(err, "Invalid argument.");
  TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  const char *err_msg = NULL;
  bool rc = ten_validate_property_json_string(json_string, &err_msg);

  if (!rc) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
    ten_rust_free_cstring(err_msg);
    return false;
  }
#endif

  return true;
}

bool ten_property_json_file_is_valid(const char *json_file, ten_error_t *err) {
  TEN_ASSERT(json_file, "Invalid argument.");
  TEN_ASSERT(err, "Invalid argument.");
  TEN_ASSERT(ten_error_check_integrity(err), "Invalid argument.");

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  const char *err_msg = NULL;
  bool rc = ten_validate_property_json_file(json_file, &err_msg);

  if (!rc) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC, err_msg);
    ten_rust_free_cstring(err_msg);
    return false;
  }
#endif

  return true;
}
