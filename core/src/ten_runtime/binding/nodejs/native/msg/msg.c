//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/msg.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/buf.h"
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

static napi_value ten_nodejs_msg_set_dest(napi_env env,
                                          napi_callback_info info) {
  const size_t argc = 5;
  napi_value args[argc];  // this, app_uri, graph_id, extension_group, extension
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

  ten_string_t app_uri;
  TEN_STRING_INIT(app_uri);

  ten_string_t graph_id;
  TEN_STRING_INIT(graph_id);

  ten_string_t extension_group;
  TEN_STRING_INIT(extension_group);

  ten_string_t extension;
  TEN_STRING_INIT(extension);

  if (!is_js_undefined(env, args[1])) {
    bool rc = ten_nodejs_get_str_from_js(env, args[1], &app_uri);
    RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get app URI", NULL);
  }

  if (!is_js_undefined(env, args[2])) {
    bool rc = ten_nodejs_get_str_from_js(env, args[2], &graph_id);
    RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get graph ID", NULL);
  }

  if (!is_js_undefined(env, args[3])) {
    bool rc = ten_nodejs_get_str_from_js(env, args[3], &extension_group);
    RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get extension group", NULL);
  }

  if (!is_js_undefined(env, args[4])) {
    bool rc = ten_nodejs_get_str_from_js(env, args[4], &extension);
    RETURN_UNDEFINED_IF_NAPI_FAIL(rc, "Failed to get extension", NULL);
  }

  bool rc = ten_msg_clear_and_set_dest(
      msg_bridge->msg, ten_string_get_raw_str(&app_uri),
      ten_string_get_raw_str(&graph_id),
      ten_string_get_raw_str(&extension_group),
      ten_string_get_raw_str(&extension), &err);
  if (!rc) {
    ten_string_t code_str;
    ten_string_init_formatted(&code_str, "%d", ten_error_code(&err));

    napi_throw_error(env, ten_string_get_raw_str(&code_str),
                     ten_error_message(&err));

    ten_string_deinit(&code_str);
  }

  ten_string_deinit(&app_uri);
  ten_string_deinit(&graph_id);
  ten_string_deinit(&extension_group);
  ten_string_deinit(&extension);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  ten_value_t *value = ten_value_from_json(c_json);

  rc = ten_msg_set_property(msg_bridge->msg, ten_string_get_raw_str(&path),
                            value, &err);
  if (!rc) {
    js_error = ten_nodejs_create_error(env, &err);
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

  return js_error ? js_error : js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
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
    js_error = js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  double value = ten_value_get_float64(c_value, &err);
  if (ten_error_code(&err) != TEN_ERROR_CODE_OK) {
    js_error = ten_nodejs_create_error(env, &err);
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
    js_error = js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_string_deinit(&value_str);
  ten_error_deinit(&err);

  return js_error ? js_error : js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  const char *value = ten_value_peek_raw_str(c_value, &err);
  if (!value) {
    js_error = ten_nodejs_create_error(env, &err);
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
    js_error = js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  bool value = ten_value_get_bool(c_value, &err);
  if (ten_error_code(&err) != TEN_ERROR_CODE_OK) {
    js_error = ten_nodejs_create_error(env, &err);
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
    js_error = js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);
  }

  ten_string_deinit(&path);
  ten_error_deinit(&err);

  return js_error ? js_error : js_null(env);
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
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  if (!ten_value_is_buf(c_value)) {
    js_error = ten_nodejs_create_error(env, &err);
    ASSERT_IF_NAPI_FAIL(js_error, "Failed to create JS error", NULL);

    goto done;
  }

  ten_buf_t *buf = ten_value_peek_buf(c_value, &err);
  if (!buf) {
    js_error = ten_nodejs_create_error(env, &err);
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
    js_error = js_null(env);
  }

  return ten_nodejs_create_result_tuple(env, js_res, js_error);
}

napi_value ten_nodejs_msg_module_init(napi_env env, napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_msg_get_name);
  EXPORT_FUNC(env, exports, ten_nodejs_msg_set_dest);
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
