//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env.h"

/*
Cross-DLL usage: ten_env_get_attached_addon is called from ten_runtime_go
static library (ten_runtime_go.a -> ten_runtime.dll). For example,
core/src/ten_runtime/binding/go/native/ten_env/ten_env_create_instance_done.c
calls ten_env_get_attached_addon().

TEN_RUNTIME_API is used for DLL export on Windows(MinGW). Without it,
error "undefined reference" will be raised.

According to GNU11 standard for inline functions, the "extern" keyword
should be used in .c file instead of .h file to prevent multiple definition.
So TEN_RUNTIME_API, which contains "extern" keyword, should be used here in .c
file.

Why only Windows(MinGW) needs this (not Linux/macOS/MSVC):
1. Linux/macOS: Global symbols declared with "extern" keyword are exported by
default.
2. Windows(MSVC): Each DLL generates and uses its own COMDAT copy of inline
functions, eliminating the need for cross-DLL imports.
*/
#if defined(__MINGW32__) || defined(__MINGW64__)

TEN_RUNTIME_API inline ten_extension_t *ten_env_get_attached_extension(
    ten_env_t *self);

TEN_RUNTIME_API inline ten_extension_group_t *
ten_env_get_attached_extension_group(ten_env_t *self);

TEN_RUNTIME_API inline ten_app_t *ten_env_get_attached_app(ten_env_t *self);

TEN_RUNTIME_API inline ten_addon_host_t *ten_env_get_attached_addon(
    ten_env_t *self);

TEN_RUNTIME_API inline ten_engine_t *ten_env_get_attached_engine(
    ten_env_t *self);

TEN_RUNTIME_API inline ten_addon_loader_t *ten_env_get_attached_addon_loader(
    ten_env_t *self);

#else

extern inline ten_extension_t *ten_env_get_attached_extension(ten_env_t *self);

extern inline ten_extension_group_t *ten_env_get_attached_extension_group(
    ten_env_t *self);

extern inline ten_app_t *ten_env_get_attached_app(ten_env_t *self);

extern inline ten_addon_host_t *ten_env_get_attached_addon(ten_env_t *self);

extern inline ten_engine_t *ten_env_get_attached_engine(ten_env_t *self);

extern inline ten_addon_loader_t *ten_env_get_attached_addon_loader(
    ten_env_t *self);

#endif
