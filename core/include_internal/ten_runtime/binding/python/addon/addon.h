//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/binding/python/common/python_stuff.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/lib/signature.h"

#define TEN_PY_ADDON_SIGNATURE 0xCFA1993E497A3D10U

typedef struct ten_py_addon_t {
  PyObject_HEAD

  ten_signature_t signature;
  TEN_ADDON_TYPE type;

  // Note that this field is _not_ a pointer, but an actual ten_addon_t entity,
  // used as the addon entity registered to the ten runtime.
  ten_addon_t c_addon;

  ten_addon_host_t *c_addon_host;
} ten_py_addon_t;

TEN_RUNTIME_PRIVATE_API bool ten_py_addon_init_for_module(PyObject *module);

TEN_RUNTIME_PRIVATE_API PyTypeObject *ten_py_addon_py_type(void);
