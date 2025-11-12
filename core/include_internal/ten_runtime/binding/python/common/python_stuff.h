//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#define PY_SSIZE_T_CLEAN

// Do not enable the debugging facility in Python even if TEN runtime is built
// in the debug mode.
#if defined(_DEBUG)
#undef _DEBUG
#include <Python.h>  // IWYU pragma: always_keep
#define _DEBUG
#else
#include <Python.h>  // IWYU pragma: always_keep
#endif
