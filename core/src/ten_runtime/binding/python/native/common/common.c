//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/python/common.h"

#include "include_internal/ten_runtime/binding/python/common/common.h"
#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_str.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

int ten_py_is_initialized(void) { return Py_IsInitialized(); }

void ten_py_initialize(void) {
  PyStatus status;

  PyPreConfig pre_config;
  PyPreConfig_InitPythonConfig(&pre_config);

  status = Py_PreInitialize(&pre_config);
  if (PyStatus_Exception(status)) {
    Py_ExitStatusException(status);
    return;
  }

  PyConfig config;
  PyConfig_InitPythonConfig(&config);
  status = Py_InitializeFromConfig(&config);

  PyConfig_Clear(&config);

  if (PyStatus_Exception(status)) {
    Py_ExitStatusException(status);
  }
}

void ten_py_initialize_with_config(const char *program,
                                   ten_list_t *module_search_path) {
  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  PyStatus status;
  if (module_search_path != NULL) {
    config.module_search_paths_set = 1;

    TEN_ASSERT(ten_list_check_integrity(module_search_path), "invalid list");
    ten_list_foreach (module_search_path, iter) {
      ten_str_listnode_t *str_node = ten_listnode_to_str_listnode(iter.node);

      const char *str = ten_string_get_raw_str(&str_node->str);

      status = PyWideStringList_Append(&config.module_search_paths,
                                       Py_DecodeLocale(str, NULL));
      if (PyStatus_Exception(status)) {
        Py_ExitStatusException(status);
      }
    }
  }

  if (program != NULL && strlen(program) > 0) {
    status = PyConfig_SetBytesString(&config, &config.program_name, program);
    if (PyStatus_Exception(status)) {
      Py_ExitStatusException(status);
    }
  }

  Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);
}

int ten_py_finalize(void) { return Py_FinalizeEx(); }

void ten_py_add_paths_to_sys(ten_list_t *paths) {
  PyObject *sys_module = PyImport_ImportModule("sys");
  if (sys_module == NULL) {
    PyErr_Print();
    return;
  }

  PyObject *sys_dict = PyModule_GetDict(sys_module);
  PyObject *sys_path = PyDict_GetItemString(sys_dict, "path");
  if (sys_path == NULL) {
    PyErr_Print();
    Py_DECREF(sys_module);
    return;
  }

  TEN_ASSERT(ten_list_check_integrity(paths), "invalid list");
  ten_list_foreach (paths, iter) {
    ten_str_listnode_t *str_node = ten_listnode_to_str_listnode(iter.node);

    const char *str = ten_string_get_raw_str(&str_node->str);
    PyObject *path = PyUnicode_FromString(str);
    PyList_Append(sys_path, path);

    Py_DECREF(path);
  }

  Py_DECREF(sys_module);
}

const char *ten_py_get_path(void) {
  wchar_t *path = Py_GetPath();
  const char *path_str = Py_EncodeLocale(path, NULL);
  return path_str;
}

void ten_py_mem_free(void *ptr) { PyMem_Free(ptr); }

void ten_py_run_simple_string(const char *code) { PyRun_SimpleString(code); }

void ten_py_run_file(const char *file_path) {
  FILE *fp = fopen(file_path, "rbe");
  PyRun_SimpleFile(fp, file_path);
}

bool ten_py_import_module(const char *module_name) {
  PyObject *module = PyUnicode_DecodeFSDefault(module_name);
  PyObject *imported_module = PyImport_Import(module);
  Py_DECREF(module);

  if (imported_module == NULL) {
    TEN_LOGI(
        "Failed to load %s. Might because of an incorrect PYTHONPATH or it is "
        "not a valid Python module.",
        module_name);
    PyErr_Print();

    return false;
  }

  return true;
}

void *ten_py_eval_save_thread(void) {
  PyThreadState *saved_py_thread_state = PyEval_SaveThread();
  return saved_py_thread_state;
}

void ten_py_eval_restore_thread(void *state) {
  TEN_ASSERT(!ten_py_is_holding_gil(), "The GIL should not be held.");
  PyEval_RestoreThread(state);
}

PyGILState_STATE ten_py_gil_state_ensure_internal(void) {
  // The logic inside PyGILState_Ensure is as follows:
  //
  // 1) Retrieves the PyThreadState for the current thread using
  //    'pthread_getspecific'.
  //    - If a PyThreadState exists, checks whether the current thread holds the
  //      GIL to determine the previous GIL state.
  //    - If no PyThreadState exists, creates a new one.
  // 2) If the current thread does not hold the GIL, calls
  //    'PyEval_RestoreThread' with the current PyThreadState.
  // 3) Returns the previous GIL state prior to this call.
  return PyGILState_Ensure();
}

void ten_py_gil_state_release_internal(PyGILState_STATE state) {
  // Acquire the 'PyThreadState' of the current thread and determine whether the
  // thread holding the GIL is the current thread. If not, it will cause a fatal
  // error.
  PyGILState_Release(state);
}

bool ten_py_is_holding_gil(void) {
  // Judge whether the current thread holds the GIL, 1: true, 0: false.
  return PyGILState_Check() == 1;
}

typedef struct ten_py_gil_state_t {
  PyGILState_STATE state;
} ten_py_gil_state_t;

void *ten_py_gil_state_ensure(void) {
  ten_py_gil_state_t *ten_py_gil_state = TEN_MALLOC(sizeof(ten_py_gil_state_t));
  TEN_ASSERT(ten_py_gil_state, "Failed to allocate memory.");

  ten_py_gil_state->state = ten_py_gil_state_ensure_internal();
  return (void *)ten_py_gil_state;
}

void ten_py_gil_state_release(void *state) {
  TEN_ASSERT(state, "Invalid argument.");

  ten_py_gil_state_t *ten_py_gil_state = state;
  ten_py_gil_state_release_internal(ten_py_gil_state->state);

  TEN_FREE(ten_py_gil_state);
}
