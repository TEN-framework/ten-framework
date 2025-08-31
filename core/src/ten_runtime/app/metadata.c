//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/app/metadata.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/app/ten_property.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/log.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/global/log.h"
#include "include_internal/ten_runtime/metadata/manifest.h"
#include "include_internal/ten_utils/log/log.h"
#include "include_internal/ten_utils/log/output.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_rust/ten_rust.h"
#endif

// Retrieve those property fields that are reserved for the TEN runtime
// under the 'ten' namespace.
ten_value_t *ten_app_get_ten_namespace_properties(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  return ten_value_object_peek(&self->property, TEN_STR_TEN);
}

bool ten_app_init_one_event_loop_per_engine(ten_app_t *self,
                                            ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_bool(value)) {
    TEN_LOGE("Invalid value type for property: %s",
             TEN_STR_ONE_EVENT_LOOP_PER_ENGINE);
    return false;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  self->one_event_loop_per_engine = ten_value_get_bool(value, &err);

  ten_error_deinit(&err);

  return true;
}

bool ten_app_init_long_running_mode(ten_app_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_bool(value)) {
    TEN_LOGE("Invalid value type for property: %s", TEN_STR_LONG_RUNNING_MODE);
    return false;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  self->long_running_mode = ten_value_get_bool(value, &err);

  ten_error_deinit(&err);

  return true;
}

bool ten_app_init_uri(ten_app_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_string(value)) {
    TEN_LOGW("Invalid uri");
    return false;
  }

  if (ten_value_peek_raw_str(value, NULL)) {
    ten_string_set_from_c_str(&self->uri, ten_value_peek_raw_str(value, NULL));
  }

  return true;
}

bool ten_app_init_deprecated_log(ten_app_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_object(value)) {
    TEN_LOGE("Invalid value type for property: %s", TEN_STR_DEPRECATED_LOG);
    return false;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *prop_kv = ten_ptr_listnode_get(iter.node);
    ten_string_t *item_key = &prop_kv->key;
    ten_value_t *item_value = prop_kv->value;

    if (ten_string_is_equal_c_str(item_key, TEN_STR_LEVEL)) {
      ten_log_global_set_output_level(ten_value_get_int64(item_value, &err));
    } else if (ten_string_is_equal_c_str(item_key, TEN_STR_FILE)) {
      ten_string_t log_file;
      TEN_STRING_INIT(log_file);

      ten_string_init_from_c_str_with_size(
          &log_file, ten_value_peek_raw_str(item_value, NULL),
          strlen(ten_value_peek_raw_str(item_value, NULL)));

      if (!ten_string_is_empty(&log_file)) {
        ten_log_global_set_output_to_file(ten_string_get_raw_str(&log_file));
      }

      ten_string_deinit(&log_file);
    } else if (ten_string_is_equal_c_str(item_key, TEN_STR_ENCRYPTION)) {
      bool enabled = false;
      ten_string_t algorithm_name;
      TEN_STRING_INIT(algorithm_name);
      ten_value_t *params = NULL;

      ten_value_object_foreach(item_value, iter) {
        ten_value_kv_t *prop_kv = ten_ptr_listnode_get(iter.node);
        ten_string_t *item_key = &prop_kv->key;
        ten_value_t *item_value = prop_kv->value;

        if (ten_string_is_equal_c_str(item_key, TEN_STR_ENABLED)) {
          enabled = ten_value_get_bool(item_value, &err);
        } else if (ten_string_is_equal_c_str(item_key, TEN_STR_ALGORITHM)) {
          ten_string_init_from_c_str_with_size(
              &algorithm_name, ten_value_peek_raw_str(item_value, NULL),
              strlen(ten_value_peek_raw_str(item_value, NULL)));
        } else if (ten_string_is_equal_c_str(item_key, TEN_STR_PARAMS)) {
          params = item_value;
        }
      }

      if (enabled) {
#if defined(TEN_ENABLE_TEN_RUST_APIS)
        ten_json_t json_params =
            TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);

        bool success = ten_value_to_json(params, &json_params);
        if (!success) {
          TEN_LOGE("Failed to convert log encryption params to JSON");
          return false;
        }

        bool must_free = false;
        const char *params_str =
            ten_json_to_string(&json_params, NULL, &must_free);

        ten_json_deinit(&json_params);

        Cipher *cipher = ten_cipher_create(
            ten_string_get_raw_str(&algorithm_name), params_str);
        ten_log_global_set_encrypt_cb(ten_encrypt_log_data, cipher);
        ten_log_global_set_encrypt_deinit_cb(ten_encrypt_log_deinit);

        if (must_free) {
          TEN_FREE(params_str);
        }
#endif
      }

      ten_string_deinit(&algorithm_name);
    }
  }

  ten_error_deinit(&err);

  return true;
}

bool ten_app_init_addon(ten_app_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_object(value)) {
    TEN_LOGE("Invalid value type for property: %s", TEN_STR_ADDON);
    return false;
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_object_foreach(value, iter) {
    ten_value_kv_t *prop_kv = ten_ptr_listnode_get(iter.node);
    ten_string_t *item_key = &prop_kv->key;
    ten_value_t *item_value = prop_kv->value;

    if (ten_string_is_equal_c_str(item_key, TEN_STR_PRELOAD_ALL)) {
      self->preload_all_addons = ten_value_get_bool(item_value, &err);
    } else {
      TEN_LOGW("Unknown property: %s", ten_string_get_raw_str(item_key));
    }
  }

  ten_error_deinit(&err);

  return true;
}

bool ten_app_init_advanced_log(ten_app_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  if (!ten_value_is_object(value)) {
    TEN_LOGE("Invalid value type for property: %s", TEN_STR_LOG);
    return false;
  }

#if defined(TEN_ENABLE_TEN_RUST_APIS)
  // Convert log config to JSON and pass it to Rust for configuration.
  ten_json_t log_config_json =
      TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(value, &log_config_json);
  if (!success) {
    TEN_LOGE("Failed to convert log config to JSON");
    return false;
  }

  bool must_free = false;
  const char *log_config_json_str =
      ten_json_to_string(&log_config_json, NULL, &must_free);

  ten_json_deinit(&log_config_json);

  char *err_msg = NULL;

  AdvancedLogConfig *log_config =
      ten_rust_create_log_config_from_json(log_config_json_str, &err_msg);
  if (log_config == NULL) {
    if (err_msg) {
      TEN_LOGE("Failed to create log config: %s", err_msg);
      ten_rust_free_cstring(err_msg);
    }
    return false;
  }

  if (must_free) {
    TEN_FREE(log_config_json_str);
  }

  err_msg = NULL;
  success = ten_rust_configure_log(
      log_config, ten_log_global_is_advanced_log_reloadable(), &err_msg);
  if (!success) {
    if (err_msg) {
      TEN_LOGE("Failed to configure log: %s", err_msg);
      ten_rust_free_cstring(err_msg);
      ten_rust_log_config_destroy(log_config);
    }

    return false;
  }

  ten_log_global_set_advanced_impl_with_config(
      ten_log_rust_log_func, ten_log_rust_config_deinit,
      ten_log_rust_config_reopen_all, log_config);

  return true;
#endif
}

static bool ten_app_determine_ten_namespace_properties(
    ten_app_t *self, ten_value_t *ten_namespace_properties) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(
      ten_namespace_properties && ten_value_is_object(ten_namespace_properties),
      "Should not happen.");

  ten_value_object_foreach(ten_namespace_properties, iter) {
    ten_value_kv_t *prop_kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(prop_kv && ten_value_kv_check_integrity(prop_kv),
               "Should not happen.");

    ten_string_t *item_key = &prop_kv->key;
    ten_value_t *item_value = prop_kv->value;

    for (int i = 0; i < ten_app_ten_namespace_prop_info_list_size; ++i) {
      const ten_app_ten_namespace_prop_info_t *prop_info =
          &ten_app_ten_namespace_prop_info_list[i];
      if (ten_string_is_equal_c_str(item_key, prop_info->name)) {
        bool rc = prop_info->init_from_value(self, item_value);
        if (rc) {
          break;
        } else {
          TEN_LOGW("Failed to init property: %s", prop_info->name);
          return false;
        }
      }
    }
  }

  return true;
}

bool ten_app_handle_ten_namespace_properties(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_value_t *ten_namespace_properties =
      ten_app_get_ten_namespace_properties(self);
  if (ten_namespace_properties == NULL) {
    return true;
  }

  TEN_ASSERT(
      ten_namespace_properties && ten_value_is_object(ten_namespace_properties),
      "Should not happen.");

  // Set default value for app properties and global log level.
  self->one_event_loop_per_engine = false;
  self->long_running_mode = false;

  // First, set the log-related configuration to default values. This way, if
  // there are no log-related properties under the `ten` namespace, the default
  // values will be used.
  ten_log_global_set_output_to_stderr();
  ten_log_global_set_output_level(DEFAULT_LOG_OUTPUT_LEVEL);

  if (!ten_app_determine_ten_namespace_properties(self,
                                                  ten_namespace_properties)) {
    return false;
  }

  return true;
}

void ten_app_handle_metadata(ten_app_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Invalid use of app %p.",
             self);

  // Load custom TEN app metadata.
  ten_metadata_load(ten_app_on_configure, self->ten_env);
}
