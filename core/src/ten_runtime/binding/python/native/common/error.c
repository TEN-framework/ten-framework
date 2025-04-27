//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common/error.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

ten_py_error_t *ten_py_error_wrap(ten_error_t *error) {
  if (!error) {
    return NULL;
  }

  ten_py_error_t *py_error = (ten_py_error_t *)ten_py_error_py_type()->tp_alloc(
      ten_py_error_py_type(), 0);
  if (!py_error) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  ten_error_init(&py_error->c_error);
  ten_error_copy(&py_error->c_error, error);

  return py_error;
}

void ten_py_error_invalidate(ten_py_error_t *py_error) {
  TEN_ASSERT(py_error, "py_error should not be NULL.");
  Py_DECREF(py_error);
}

void ten_py_error_destroy(PyObject *self) {
  ten_py_error_t *py_error = (ten_py_error_t *)self;
  if (!py_error) {
    return;
  }

  ten_error_deinit(&py_error->c_error);

  Py_TYPE(self)->tp_free(self);
}

PyObject *ten_py_error_get_error_code(PyObject *self, PyObject *args) {
  ten_py_error_t *py_error = (ten_py_error_t *)self;
  if (!py_error) {
    return ten_py_raise_py_value_error_exception("Invalid argument.");
  }

  return PyLong_FromLong(ten_error_code(&py_error->c_error));
}

PyObject *ten_py_error_get_error_message(PyObject *self, PyObject *args) {
  ten_py_error_t *py_error = (ten_py_error_t *)self;
  if (!py_error) {
    return ten_py_raise_py_value_error_exception("Invalid argument.");
  }

  return PyUnicode_FromString(ten_error_message(&py_error->c_error));
}

static void ten_py_print_py_error(void) {
  PyObject *ptype = NULL;
  PyObject *pvalue = NULL;
  PyObject *ptraceback = NULL;

  PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  if (!ptype && !pvalue && !ptraceback) {
    // No error to fetch.
    return;
  }

  PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
  if (pvalue != NULL) {
    PyObject *py_err_msg = PyObject_Str(pvalue);
    if (!py_err_msg) {
      TEN_LOGE("Failed to convert exception value to string.");
    } else {
      const char *err_msg = PyUnicode_AsUTF8(py_err_msg);
      if (!err_msg) {
        TEN_LOGE("Failed to encode exception message as UTF-8.");
      } else {
        const char *py_exception_type = PyExceptionClass_Name(ptype);
        if (!py_exception_type) {
          py_exception_type = "Unknown Exception";
        }
        TEN_LOGE("%s: %s", py_exception_type, err_msg);
      }
      Py_DECREF(py_err_msg);
    }
  } else {
    TEN_LOGE("Failed to get exception value.");
  }

  if (ptraceback) {
    PyObject *stderr_file = PySys_GetObject("stderr");
    if (stderr_file) {
      // Dump the call stack of python codes to stderr.
      PyTraceBack_Print(ptraceback, stderr_file);
    } else {
      TEN_LOGW("Failed to get stderr to dump backtrace.");
    }
  }

  Py_XDECREF(ptype);
  Py_XDECREF(pvalue);
  Py_XDECREF(ptraceback);
}

bool ten_py_check_and_clear_py_error(void) {
  bool err_occurred = PyErr_Occurred();
  if (err_occurred) {
    ten_py_print_py_error();

    PyErr_Clear();
  }
  return err_occurred;
}

PyObject *ten_py_raise_py_value_error_exception(const char *msg, ...) {
  ten_string_t err_msg;
  TEN_STRING_INIT(err_msg);

  va_list args;
  va_start(args, msg);
  ten_string_append_from_va_list(&err_msg, msg, args);
  va_end(args);

  TEN_LOGD("Raise Python ValueError exception: %s",
           ten_string_get_raw_str(&err_msg));
  PyErr_SetString(PyExc_ValueError, ten_string_get_raw_str(&err_msg));

  ten_string_deinit(&err_msg);

  // Returning NULL indicates that an exception has occurred during the
  // function's execution, and signaling to the Python interpreter to handle the
  // exception.
  return NULL;
}

PyObject *ten_py_raise_py_type_error_exception(const char *msg) {
  TEN_LOGD("Raise Python TypeError exception: %s", msg);
  PyErr_SetString(PyExc_TypeError, msg);

  return NULL;
}

PyObject *ten_py_raise_py_memory_error_exception(const char *msg) {
  TEN_LOGD("Raise Python TypeError exception: %s", msg);
  PyErr_SetString(PyExc_MemoryError, msg);

  return NULL;
}

PyObject *ten_py_raise_py_system_error_exception(const char *msg) {
  TEN_LOGD("Raise Python SystemError exception: %s", msg);
  PyErr_SetString(PyExc_SystemError, msg);

  return NULL;
}

PyObject *ten_py_raise_py_import_error_exception(const char *msg) {
  TEN_LOGD("Raise Python ImportError exception: %s", msg);
  PyErr_SetString(PyExc_ImportError, msg);

  return NULL;
}

PyObject *ten_py_raise_py_runtime_error_exception(const char *msg) {
  TEN_LOGD("Raise Python RuntimeError exception: %s", msg);
  PyErr_SetString(PyExc_RuntimeError, msg);

  return NULL;
}

PyObject *ten_py_raise_py_not_implemented_error_exception(const char *msg) {
  TEN_LOGD("Raise Python NotImplementedError exception: %s", msg);
  PyErr_SetString(PyExc_NotImplementedError, msg);

  return NULL;
}

bool ten_py_error_init_for_module(PyObject *module) {
  PyTypeObject *py_type = ten_py_error_py_type();

  if (PyType_Ready(py_type) < 0) {
    ten_py_raise_py_system_error_exception("Python Error class is not ready.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  if (PyModule_AddObjectRef(module, "_TenError", (PyObject *)py_type) < 0) {
    ten_py_raise_py_import_error_exception(
        "Failed to add Python type to module.");

    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  return true;
}
