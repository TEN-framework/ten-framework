//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/msg/msg.h"

#include "include_internal/ten_runtime/binding/python/common/error.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/common/loc.h"
#include "ten_runtime/msg/msg.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_json.h"

static PyTypeObject *ten_py_msg_type = NULL;

bool ten_py_msg_check_integrity(ten_py_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PY_MSG_SIGNATURE) {
    return false;
  }

  return true;
}

void ten_py_msg_destroy_c_msg(ten_py_msg_t *self) {
  if (self->c_msg) {
    ten_shared_ptr_destroy(self->c_msg);
    self->c_msg = NULL;
  }
}

ten_shared_ptr_t *ten_py_msg_move_c_msg(ten_py_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_shared_ptr_t *c_msg = self->c_msg;
  self->c_msg = NULL;

  return c_msg;
}

PyObject *ten_py_msg_get_name(PyObject *self, TEN_UNUSED PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *name = ten_msg_get_name(c_msg);

  PyObject *res = Py_BuildValue("s", name);

  return res;
}

PyObject *ten_py_msg_set_name(PyObject *self, TEN_UNUSED PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *name = NULL;
  if (!PyArg_ParseTuple(args, "s", &name)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_msg_set_name(c_msg, name, &err);

  if (!rc) {
    ten_py_raise_py_value_error_exception(ten_error_message(&err));
  }

  ten_error_deinit(&err);

  if (rc) {
    Py_RETURN_NONE;
  } else {
    return NULL;
  }
}

PyObject *ten_py_msg_get_source(PyObject *self, TEN_UNUSED PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;
  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  ten_loc_t *loc = ten_msg_get_src_loc(c_msg);
  TEN_ASSERT(loc, "Should not happen.");
  if (!loc) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *app_uri = ten_string_get_raw_str(&loc->app_uri);
  const char *graph_id = ten_string_get_raw_str(&loc->graph_id);
  const char *extension_name = ten_string_get_raw_str(&loc->extension_name);

  PyObject *py_app_uri = Py_None;
  PyObject *py_graph_id = Py_None;
  PyObject *py_extension_name = Py_None;

  if (loc->has_app_uri) {
    py_app_uri =
        (app_uri && app_uri[0]) ? PyUnicode_FromString(app_uri) : Py_None;
  }
  if (loc->has_graph_id) {
    py_graph_id =
        (graph_id && graph_id[0]) ? PyUnicode_FromString(graph_id) : Py_None;
  }
  if (loc->has_extension_name) {
    py_extension_name = (extension_name && extension_name[0])
                            ? PyUnicode_FromString(extension_name)
                            : Py_None;
  }

  PyObject *res =
      Py_BuildValue("(OOO)", py_app_uri, py_graph_id, py_extension_name);

  if (py_app_uri != Py_None) {
    Py_DECREF(py_app_uri);
  }
  if (py_graph_id != Py_None) {
    Py_DECREF(py_graph_id);
  }
  if (py_extension_name != Py_None) {
    Py_DECREF(py_extension_name);
  }

  return res;
}

typedef struct {
  const char *app_uri;
  const char *graph_id;
  const char *extension_name;
} dest_info_t;

PyObject *ten_py_msg_set_dests(PyObject *self, TEN_UNUSED PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  if (PyTuple_GET_SIZE(args) != 1) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when set_dests_internal.");
  }

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  PyObject *dest_list = NULL;
  if (!PyArg_ParseTuple(args, "O", &dest_list)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  // Check if the argument is a list
  if (!PyList_Check(dest_list)) {
    return ten_py_raise_py_value_error_exception(
        "Expected a list of destination tuples.");
  }

  Py_ssize_t list_size = PyList_Size(dest_list);
  if (list_size == 0) {
    // Empty list, just clear destinations
    ten_msg_clear_dest(c_msg);
    Py_RETURN_NONE;
  }

  // Allocate array to store destination information
  dest_info_t *dest_infos = TEN_MALLOC(sizeof(dest_info_t) * list_size);
  TEN_ASSERT(dest_infos, "Failed to allocate memory.");
  if (!dest_infos) {
    return ten_py_raise_py_value_error_exception("Failed to allocate memory.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Phase 1: Parse all destination tuples and store string pointers
  for (Py_ssize_t i = 0; i < list_size; i++) {
    PyObject *dest_tuple = PyList_GetItem(dest_list, i);

    // Check if each item is a tuple
    if (!PyTuple_Check(dest_tuple)) {
      TEN_FREE(dest_infos);
      ten_error_deinit(&err);
      return ten_py_raise_py_value_error_exception(
          "Expected tuple in destination list.");
    }

    // Check if tuple has exactly 3 elements
    if (PyTuple_GET_SIZE(dest_tuple) != 3) {
      TEN_FREE(dest_infos);
      ten_error_deinit(&err);
      return ten_py_raise_py_value_error_exception(
          "Each destination tuple must have exactly 3 elements.");
    }

    // Parse the tuple (app_uri, graph_id, extension_name)
    if (!PyArg_ParseTuple(dest_tuple, "zzz", &dest_infos[i].app_uri,
                          &dest_infos[i].graph_id,
                          &dest_infos[i].extension_name)) {
      TEN_FREE(dest_infos);
      ten_error_deinit(&err);
      return ten_py_raise_py_value_error_exception(
          "Failed to parse destination tuple.");
    }
  }

  // Phase 2: Validate all locations
  for (Py_ssize_t i = 0; i < list_size; i++) {
    if (!ten_loc_str_check_correct(dest_infos[i].app_uri,
                                   dest_infos[i].graph_id,
                                   dest_infos[i].extension_name, &err)) {
      PyObject *result = (PyObject *)ten_py_error_wrap(&err);
      TEN_FREE(dest_infos);
      ten_error_deinit(&err);
      return result;
    }
  }

  // Phase 3: All validations passed, now clear and add destinations
  ten_msg_clear_dest(c_msg);
  for (Py_ssize_t i = 0; i < list_size; i++) {
    ten_msg_add_dest(c_msg, dest_infos[i].app_uri, dest_infos[i].graph_id,
                     dest_infos[i].extension_name);
  }

  TEN_FREE(dest_infos);
  ten_error_deinit(&err);
  Py_RETURN_NONE;
}

PyObject *ten_py_msg_set_property_string(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  const char *value = NULL;

  if (!PyArg_ParseTuple(args, "ss", &path, &value)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");
  TEN_ASSERT(value, "value should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *c_value = ten_value_create_string(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_msg_set_property(c_msg, path, c_value, &err);
  if (!rc) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_value_destroy(c_value);
    ten_error_deinit(&err);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_msg_get_property_string(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const char *default_value = "";

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  if (!ten_value_is_string(c_value)) {
    ten_error_set(&err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                  "Value is not string.");
    goto error;
  }

  const char *value = ten_value_peek_raw_str(c_value, &err);
  PyObject *res = Py_BuildValue("(sO)", value, Py_None);
  ten_error_deinit(&err);
  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(sO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_set_property_from_json(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  const char *json_str = NULL;

  if (!PyArg_ParseTuple(args, "zs", &path, &json_str)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_json_t *c_json = ten_json_from_string(json_str, &err);
  if (!c_json) {
    goto error;
  }

  ten_value_t *value = ten_value_from_json(c_json);
  TEN_ASSERT(value, "value should not be NULL.");

  ten_json_destroy(c_json);

  bool rc = ten_msg_set_property(c_msg, path, value, &err);
  if (!rc) {
    ten_value_destroy(value);
    goto error;
  }

  ten_error_deinit(&err);
  Py_RETURN_NONE;

error: {
  PyObject *result = (PyObject *)ten_py_error_wrap(&err);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_get_property_to_json(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  if (PyTuple_GET_SIZE(args) == 1) {
    if (!PyArg_ParseTuple(args, "z", &path)) {
      return ten_py_raise_py_value_error_exception(
          "Failed to parse arguments.");
    }
  } else if (PyTuple_GET_SIZE(args) != 0) {
    return ten_py_raise_py_value_error_exception(
        "Invalid argument count when msg.get_property_to_json.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const char *default_value = "";

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  ten_json_t c_json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
  bool success = ten_value_to_json(c_value, &c_json);
  TEN_ASSERT(success, "Should not happen.");

  bool must_free = true;
  const char *json_string = ten_json_to_string(&c_json, NULL, &must_free);
  PyObject *res = Py_BuildValue("(sO)", json_string, Py_None);
  if (must_free) {
    TEN_FREE(json_string);
  }

  ten_json_deinit(&c_json);
  ten_error_deinit(&err);

  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(sO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_get_property_int(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const int64_t default_value = 0;

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  int64_t value = ten_value_get_int64(c_value, &err);
  if (!ten_error_is_success(&err)) {
    goto error;
  }

  PyObject *res = Py_BuildValue("(lO)", value, Py_None);

  ten_error_deinit(&err);

  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(lO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_set_property_int(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  int64_t value = 0;

  if (!PyArg_ParseTuple(args, "sl", &path, &value)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *c_value = ten_value_create_int64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_msg_set_property(c_msg, path, c_value, &err);
  if (!rc) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    ten_value_destroy(c_value);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_msg_get_property_bool(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const bool default_value = false;

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  bool value = ten_value_get_bool(c_value, &err);
  if (!ten_error_is_success(&err)) {
    goto error;
  }

  PyObject *py_value = PyBool_FromLong(value);
  PyObject *res = Py_BuildValue("(OO)", py_value, Py_None);
  Py_DECREF(py_value);
  ten_error_deinit(&err);

  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *py_value = PyBool_FromLong(default_value);
  PyObject *result = Py_BuildValue("(OO)", py_value, py_error);
  ten_py_error_invalidate(py_error);
  Py_DECREF(py_value);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_set_property_bool(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  int value = 0;

  if (!PyArg_ParseTuple(args, "si", &path, &value)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *c_value = ten_value_create_bool(value > 0);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_msg_set_property(c_msg, path, c_value, &err);
  if (!rc) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    ten_value_destroy(c_value);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_msg_get_property_float(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const double default_value = 0.0;

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  double value = ten_value_get_float64(c_value, &err);
  if (!ten_error_is_success(&err)) {
    goto error;
  }

  PyObject *res = Py_BuildValue("(dO)", value, Py_None);
  ten_error_deinit(&err);

  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *result = Py_BuildValue("(dO)", default_value, py_error);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_set_property_float(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  double value = 0.0;

  if (!PyArg_ParseTuple(args, "sd", &path, &value)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_t *c_value = ten_value_create_float64(value);
  TEN_ASSERT(c_value, "Should not happen.");

  bool rc = ten_msg_set_property(c_msg, path, c_value, &err);
  if (!rc) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    ten_value_destroy(c_value);
    return result;
  }

  ten_error_deinit(&err);

  Py_RETURN_NONE;
}

PyObject *ten_py_msg_get_property_buf(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;
  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  ten_error_t err;
  TEN_ERROR_INIT(err);
  const char *default_value = "";

  ten_value_t *c_value = ten_msg_peek_property(c_msg, path, &err);
  if (!c_value) {
    goto error;
  }

  ten_buf_t *buf = ten_value_peek_buf(c_value, &err);
  if (!buf) {
    goto error;
  }

  TEN_ASSERT(ten_buf_check_integrity(buf), "Invalid buf.");

  PyObject *py_value = PyByteArray_FromStringAndSize((const char *)buf->data,
                                                     (Py_ssize_t)buf->size);
  PyObject *res = Py_BuildValue("(OO)", py_value, Py_None);
  Py_DECREF(py_value);
  ten_error_deinit(&err);

  return res;

error: {
  ten_py_error_t *py_error = ten_py_error_wrap(&err);
  PyObject *py_value = PyByteArray_FromStringAndSize(default_value, 0);
  PyObject *result = Py_BuildValue("(OO)", py_value, py_error);
  Py_DECREF(py_value);
  ten_py_error_invalidate(py_error);
  ten_error_deinit(&err);
  return result;
}
}

PyObject *ten_py_msg_set_property_buf(PyObject *self, PyObject *args) {
  ten_py_msg_t *py_msg = (ten_py_msg_t *)self;

  TEN_ASSERT(py_msg, "Invalid argument.");
  TEN_ASSERT(ten_py_msg_check_integrity(py_msg), "Invalid argument.");

  ten_shared_ptr_t *c_msg = py_msg->c_msg;
  if (!c_msg) {
    return ten_py_raise_py_value_error_exception("Msg is invalidated.");
  }

  const char *path = NULL;
  Py_buffer py_buf;

  if (!PyArg_ParseTuple(args, "sy*", &path, &py_buf)) {
    return ten_py_raise_py_value_error_exception("Failed to parse arguments.");
  }

  TEN_ASSERT(path, "path should not be NULL.");

  Py_ssize_t size = 0;
  uint8_t *data = py_buf.buf;
  if (!data) {
    return ten_py_raise_py_value_error_exception("Invalid buffer.");
  }

  size = py_buf.len;
  if (size <= 0) {
    return ten_py_raise_py_value_error_exception("Invalid buffer size.");
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_buf_t buf;
  ten_buf_init_with_owned_data(&buf, size);

  memcpy(buf.data, data, size);

  ten_value_t *c_value = ten_value_create_buf_with_move(buf);
  TEN_ASSERT(c_value && ten_value_check_integrity(c_value),
             "Failed to create value.");

  bool rc = ten_msg_set_property(c_msg, path, c_value, &err);
  if (!rc) {
    PyObject *result = (PyObject *)ten_py_error_wrap(&err);
    ten_error_deinit(&err);
    PyBuffer_Release(&py_buf);
    ten_value_destroy(c_value);
    return result;
  }

  ten_error_deinit(&err);
  PyBuffer_Release(&py_buf);

  Py_RETURN_NONE;
}

bool ten_py_msg_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_msg_py_type();

  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception("Python Msg class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_Msg", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }
  return true;
}

PyObject *ten_py_msg_register_msg_type(TEN_UNUSED PyObject *self,
                                       PyObject *args) {
  PyObject *cls = NULL;
  if (!PyArg_ParseTuple(args, "O!", &PyType_Type, &cls)) {
    return NULL;
  }

  Py_XINCREF(cls);
  Py_XDECREF(ten_py_msg_type);

  ten_py_msg_type = (PyTypeObject *)cls;

  Py_RETURN_NONE;
}
