//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "ten_utils/lib/signature.h"

#define TEN_PY_EXTENSION_TESTER_SIGNATURE 0x2B343E0B87397B5FU

typedef struct ten_py_extension_tester_t {
  PyObject_HEAD
  ten_signature_t signature;
  ten_extension_tester_t *c_extension_tester;

  // Companion ten_env_tester object, the actual type is
  // ten_py_ten_env_tester_t.
  PyObject *py_ten_env_tester;
} ten_py_extension_tester_t;

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_extension_tester_py_type(void);

TEN_RUNTIME_API bool ten_py_extension_tester_init_for_module(PyObject *module);
