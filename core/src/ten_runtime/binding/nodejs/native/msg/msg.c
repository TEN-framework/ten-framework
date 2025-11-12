//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/msg.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/binding/nodejs/error/error.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/common/loc.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

void ten_nodejs_msg_init_from_c_msg(ten_nodejs_msg_t *self,
                                    ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  ten_signature_set(&self->signature, TEN_NODEJS_MSG_SIGNATURE);

  self->msg = ten_shared_ptr_clone(msg);
}

void ten_nodejs_msg_deinit(ten_nodejs_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (self->msg) {
    ten_shared_ptr_destroy(self->msg);
    self->msg = NULL;
  }

  ten_signature_set(&self->signature, 0);
}

static napi_value ten_nodejs_msg_get_name(napi_env env,
                                          napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_shared_ptr_t *msg = msg_bridge->msg;
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  const char *name = ten_msg_get_name(msg);
  TEN_ASSERT(name, "Should not happen.");

  napi_value js_msg_name = NULL;
  status = napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &js_msg_name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_msg_name != NULL,
                                "Failed to create JS string: %d", status);

  return js_msg_name;
}

static napi_value ten_nodejs_msg_set_dests(napi_env env,
                                           napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, dests
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  napi_value dests_array = args[1];

  // Check if dests is an array
  bool is_array = false;
  status = napi_is_array(env, dests_array, &is_array);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && is_array,
                                "dests parameter must be an array", NULL);

  // Get array length
  uint32_t array_length = 0;
  status = napi_get_array_length(env, dests_array, &array_length);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to get array length: %d", status);

  if (array_length == 0) {
    // Empty array, just clear destinations
    ten_msg_clear_dest(msg_bridge->msg);
    return js_undefined(env);
  }

  // Allocate array to store destination information
  ten_loc_t *dest_locs = TEN_MALLOC(sizeof(ten_loc_t) * array_length);
  TEN_ASSERT(dest_locs, "Failed to allocate memory.");
  if (!dest_locs) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to allocate memory.",
                     NAPI_AUTO_LENGTH);
    return js_undefined(env);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Phase 1: Parse all destination objects and store string information
  for (uint32_t i = 0; i < array_length; i++) {
    napi_value loc_element = NULL;
    status = napi_get_element(env, dests_array, i, &loc_element);
    if (status != napi_ok || loc_element == NULL) {
      // Clean up previously initialized strings
      for (uint32_t j = 0; j < i; j++) {
        ten_loc_deinit(&dest_locs[j]);
      }
      TEN_FREE(dest_locs);
      ten_error_deinit(&err);
      napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get array element",
                       NAPI_AUTO_LENGTH);
      return js_undefined(env);
    }

    // Initialize strings
    ten_loc_init_empty(&dest_locs[i]);

    // Extract appUri property
    napi_value app_uri_val =
        ten_nodejs_get_property(env, loc_element, "appUri");
    if (!is_js_undefined(env, app_uri_val)) {
      bool rc =
          ten_nodejs_get_str_from_js(env, app_uri_val, &dest_locs[i].app_uri);
      if (!rc) {
        // Clean up all initialized strings
        for (uint32_t j = 0; j <= i; j++) {
          ten_loc_deinit(&dest_locs[j]);
        }
        TEN_FREE(dest_locs);

        ten_error_deinit(&err);
        napi_throw_error(env, NULL, "Failed to get appUri from Loc");
        return js_undefined(env);
      } else {
        dest_locs[i].has_app_uri = true;
      }
    }

    // Extract graphId property
    napi_value graph_id_val =
        ten_nodejs_get_property(env, loc_element, "graphId");
    if (!is_js_undefined(env, graph_id_val)) {
      bool rc =
          ten_nodejs_get_str_from_js(env, graph_id_val, &dest_locs[i].graph_id);
      if (!rc) {
        // Clean up all initialized strings
        for (uint32_t j = 0; j <= i; j++) {
          ten_loc_deinit(&dest_locs[j]);
        }
        TEN_FREE(dest_locs);

        ten_error_deinit(&err);
        napi_throw_error(env, NULL, "Failed to get graphId from Loc");
        return js_undefined(env);
      } else {
        dest_locs[i].has_graph_id = true;
      }
    }

    // Extract extensionName property
    napi_value extension_name_val =
        ten_nodejs_get_property(env, loc_element, "extensionName");
    if (!is_js_undefined(env, extension_name_val)) {
      bool rc = ten_nodejs_get_str_from_js(env, extension_name_val,
                                           &dest_locs[i].extension_name);
      if (!rc) {
        // Clean up all initialized strings
        for (uint32_t j = 0; j <= i; j++) {
          ten_loc_deinit(&dest_locs[j]);
        }
        TEN_FREE(dest_locs);

        ten_error_deinit(&err);
        napi_throw_error(env, NULL, "Failed to get extensionName from Loc");
        return js_undefined(env);
      } else {
        dest_locs[i].has_extension_name = true;
      }
    }
  }

  // Phase 2: Validate all locations
  for (uint32_t i = 0; i < array_length; i++) {
    if (!ten_loc_str_check_correct(
            dest_locs[i].has_app_uri
                ? ten_string_get_raw_str(&dest_locs[i].app_uri)
                : NULL,
            dest_locs[i].has_graph_id
                ? ten_string_get_raw_str(&dest_locs[i].graph_id)
                : NULL,
            dest_locs[i].has_extension_name
                ? ten_string_get_raw_str(&dest_locs[i].extension_name)
                : NULL,
            &err)) {
      // Clean up all strings
      for (uint32_t j = 0; j < array_length; j++) {
        ten_loc_deinit(&dest_locs[j]);
      }
      TEN_FREE(dest_locs);

      napi_value js_error = ten_nodejs_error_wrap(env, &err);
      RETURN_UNDEFINED_IF_NAPI_FAIL(js_error, "Failed to create JS error");

      ten_error_deinit(&err);

      return js_error;
    }
  }

  // Phase 3: All validations passed, now clear and add destinations
  ten_msg_clear_dest(msg_bridge->msg);
  for (uint32_t i = 0; i < array_length; i++) {
    ten_msg_add_dest(msg_bridge->msg,
                     dest_locs[i].has_app_uri
                         ? ten_string_get_raw_str(&dest_locs[i].app_uri)
                         : NULL,
                     dest_locs[i].has_graph_id
                         ? ten_string_get_raw_str(&dest_locs[i].graph_id)
                         : NULL,
                     dest_locs[i].has_extension_name
                         ? ten_string_get_raw_str(&dest_locs[i].extension_name)
                         : NULL);
  }

  // Clean up all strings
  for (uint32_t i = 0; i < array_length; i++) {
    ten_loc_deinit(&dest_locs[i]);
  }
  TEN_FREE(dest_locs);
  ten_error_deinit(&err);

  return js_undefined(env);
}

static napi_value ten_nodejs_msg_set_property_from_json(
    napi_env env, napi_callback_info info) {
  napi_value js_error = NULL;

  const size_t argc = 3;
  napi_value args[argc];  // this, path, json_str

  // If the function call fails, throw an exception directly, not expected to be
  // caught by developers
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  ten_json_t *c_json = NULL;

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_string_t json_str;
  TEN_STRING_INIT(json_str);

  rc = ten_nodejs_get_str_from_js(env, args[2], &json_str);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property value JSON", NULL);

  c_json = ten_json_from_string(ten_string_get_raw_str(&json_str), &err);
  if (!c_json) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  ten_value_t *value = ten_value_from_json(c_json);

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            value, &err);
  if (!rc) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

done:
  ten_string_deinit(&path);
  ten_string_deinit(&json_str);
  ten_error_deinit(&err);
  if (c_json) {
    ten_json_destroy(c_json);
  }

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_msg_get_property_to_json(napi_env env,
                                                      napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, path
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  napi_value js_res = NULL;
  napi_value js_error = NULL;

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_msg_peek_property(
      msg_bridge->msg, ten_string_get_raw_str(&path), &err);
  if (!c_value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  ten_json_t c_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(c_value, &c_json);
  TEN_ASSERT(success, "Should not happen.");

  bool must_free = false;
  const char *json_str = ten_json_to_string(&c_json, NULL, &must_free);
  TEN_ASSERT(json_str, "Should not happen.");

  ten_json_deinit(&c_json);

  status = napi_create_string_utf8(env, json_str, NAPI_AUTO_LENGTH, &js_res);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                "Failed to create JS string: %d", status);

  if (must_free) {
    TEN_FREE(json_str);
  }

done:
  ten_string_deinit(&path);
  ten_error_deinit(&err);

  if (!js_res) {
    status = napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &js_res);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                  "Failed to create JS string: %d", status);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

static napi_value ten_nodejs_msg_set_property_number(napi_env env,
                                                     napi_callback_info info) {
  napi_value js_error = NULL;
  const size_t argc = 3;
  napi_value args[argc];  // this, path, value
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  napi_value js_value = args[2];
  double value = 0;
  status = napi_get_value_double(env, js_value, &value);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok, "Failed to get value", NULL);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_value_create_float64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            c_value, &err);
  if (!rc) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_msg_get_property_number(napi_env env,
                                                     napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, path
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  napi_value js_res = NULL;
  napi_value js_error = NULL;

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_msg_peek_property(
      msg_bridge->msg, ten_string_get_raw_str(&path), &err);
  if (!c_value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  double value = ten_value_get_float64(c_value, &err);
  if (ten_error_code(&err) != TEN_ERROR_CODE_OK) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  status = napi_create_double(env, value, &js_res);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                "Failed to create JS number: %d", status);

done:
  ten_string_deinit(&path);
  ten_error_deinit(&err);

  if (!js_res) {
    status = napi_create_double(env, 0, &js_res);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                  "Failed to create JS number: %d", status);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

static napi_value ten_nodejs_msg_set_property_string(napi_env env,
                                                     napi_callback_info info) {
  napi_value js_error = NULL;

  const size_t argc = 3;
  napi_value args[argc];  // this, path, value
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_string_t value_str;
  TEN_STRING_INIT(value_str);

  rc = ten_nodejs_get_str_from_js(env, args[2], &value_str);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property value", NULL);

  ten_value_t *c_value =
      ten_value_create_string(ten_string_get_raw_str(&value_str));
  TEN_ASSERT(c_value, "Failed to create string value.");

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            c_value, &err);
  if (!rc) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_string_deinit(&value_str);
  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_msg_get_property_string(napi_env env,
                                                     napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, path
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  napi_value js_res = NULL;
  napi_value js_error = NULL;
  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_msg_peek_property(
      msg_bridge->msg, ten_string_get_raw_str(&path), &err);
  if (!c_value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  const char *value = ten_value_peek_raw_str(c_value, &err);
  if (!value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  status = napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &js_res);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                "Failed to create JS string: %d", status);

done:
  ten_string_deinit(&path);
  ten_error_deinit(&err);

  if (!js_res) {
    status = napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &js_res);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                  "Failed to create JS string: %d", status);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

static napi_value ten_nodejs_msg_set_property_bool(napi_env env,
                                                   napi_callback_info info) {
  napi_value js_error = NULL;

  const size_t argc = 3;
  napi_value args[argc];  // this, path, value
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  napi_value js_value = args[2];
  bool value = false;
  status = napi_get_value_bool(env, js_value, &value);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok, "Failed to get value", NULL);

  ten_value_t *c_value = ten_value_create_bool(value);
  TEN_ASSERT(c_value, "Failed to create bool value.");

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            c_value, &err);
  if (!rc) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_msg_get_property_bool(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, path
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  napi_value js_res = NULL;
  napi_value js_error = NULL;

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_msg_peek_property(
      msg_bridge->msg, ten_string_get_raw_str(&path), &err);
  if (!c_value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  bool value = ten_value_get_bool(c_value, &err);
  if (ten_error_code(&err) != TEN_ERROR_CODE_OK) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  status = napi_get_boolean(env, value, &js_res);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                "Failed to create JS boolean: %d", status);

done:
  ten_string_deinit(&path);
  ten_error_deinit(&err);

  if (!js_res) {
    status = napi_get_boolean(env, false, &js_res);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                  "Failed to create JS boolean: %d", status);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

static napi_value ten_nodejs_msg_set_property_buf(napi_env env,
                                                  napi_callback_info info) {
  napi_value js_error = NULL;

  const size_t argc = 3;
  napi_value args[argc];  // this, path, value
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  void *data = NULL;
  size_t size = 0;
  status = napi_get_arraybuffer_info(env, args[2], &data, &size);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok, "Failed to get buffer",
                                NULL);

  ten_buf_t buf;
  ten_buf_init_with_copying_data(&buf, data, size);

  ten_value_t *c_value = ten_value_create_buf_with_move(buf);
  TEN_ASSERT(c_value && ten_value_check_integrity(c_value),
             "Failed to create buffer value.");

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            c_value, &err);
  if (!rc) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_undefined(env);
}

static napi_value ten_nodejs_msg_get_property_buf(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, path
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_string_t path;
  TEN_STRING_INIT(path);

  napi_value js_res = NULL;
  napi_value js_error = NULL;

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &path);
  RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get property path", NULL);

  ten_value_t *c_value = ten_msg_peek_property(
      msg_bridge->msg, ten_string_get_raw_str(&path), &err);
  if (!c_value) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  if (!ten_value_is_buf(c_value)) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  ten_buf_t *buf = ten_value_peek_buf(c_value, &err);
  if (!buf) {
    js_error = ten_nodejs_error_wrap(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  status = napi_create_buffer_copy(env, ten_buf_get_size(buf),
                                   ten_buf_get_data(buf), NULL, &js_res);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                "Failed to create JS buffer: %d", status);

done:
  ten_string_deinit(&path);
  ten_error_deinit(&err);

  if (!js_res) {
    status = napi_create_buffer_copy(env, 0, NULL, NULL, &js_res);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_res != NULL,
                                  "Failed to create JS buffer: %d", status);
  }

  if (!js_error) {
    js_error = js_undefined(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

static napi_value ten_nodejs_msg_get_source(napi_env env,
                                            napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return js_undefined(env);
  }

  ten_nodejs_msg_t *msg_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&msg_bridge);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && msg_bridge != NULL,
                                "Failed to get msg bridge: %d", status);
  TEN_ASSERT(msg_bridge, "Should not happen.");

  ten_shared_ptr_t *msg = msg_bridge->msg;
  TEN_ASSERT(msg, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Should not happen.");

  ten_loc_t *loc = ten_msg_get_src_loc(msg);
  TEN_ASSERT(loc, "Should not happen.");
  if (!loc) {
    napi_throw_error(env, NULL, "Failed to get msg source location");
    return js_undefined(env);
  }

  const char *app_uri = ten_string_get_raw_str(&loc->app_uri);
  const char *graph_id = ten_string_get_raw_str(&loc->graph_id);
  const char *extension_name = ten_string_get_raw_str(&loc->extension_name);

  napi_value js_app_uri = NULL;
  napi_value js_graph_id = NULL;
  napi_value js_extension_name = NULL;

  if (loc->has_app_uri) {
    status = napi_create_string_utf8(env, app_uri ? app_uri : "",
                                     NAPI_AUTO_LENGTH, &js_app_uri);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_app_uri != NULL,
                                  "Failed to create JS string for app_uri: %d",
                                  status);
  } else {
    js_app_uri = js_undefined(env);
  }

  if (loc->has_graph_id) {
    status = napi_create_string_utf8(env, graph_id ? graph_id : "",
                                     NAPI_AUTO_LENGTH, &js_graph_id);
    RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_graph_id != NULL,
                                  "Failed to create JS string for graph_id: %d",
                                  status);
  } else {
    js_graph_id = js_undefined(env);
  }

  if (loc->has_extension_name) {
    status = napi_create_string_utf8(env, extension_name ? extension_name : "",
                                     NAPI_AUTO_LENGTH, &js_extension_name);
    RETURN_UNDEFINED_IF_NAPI_FAIL(
        status == napi_ok && js_extension_name != NULL,
        "Failed to create JS string for extension_name: %d", status);
  } else {
    js_extension_name = js_undefined(env);
  }

  napi_value js_array = NULL;
  status = napi_create_array_with_length(env, 3, &js_array);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok && js_array != NULL,
                                "Failed to create JS array: %d", status);

  status = napi_set_element(env, js_array, 0, js_app_uri);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to set JS array element 0: %d", status);

  status = napi_set_element(env, js_array, 1, js_graph_id);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to set JS array element 1: %d", status);

  status = napi_set_element(env, js_array, 2, js_extension_name);
  RETURN_UNDEFINED_IF_NAPI_FAIL(status == napi_ok,
                                "Failed to set JS array element 2: %d", status);

  return js_array;
}

napi_value ten_nodejs_msg_module_init(napi_env env, napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_name);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_source);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_dests);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_property_from_json);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_property_to_json);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_property_number);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_property_number);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_property_string);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_property_string);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_property_bool);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_property_bool);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_property_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_property_buf);

  return exports;
}
