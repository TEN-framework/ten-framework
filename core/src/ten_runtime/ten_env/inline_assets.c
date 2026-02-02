//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env.h"

#include "ten_utils/macro/check.h"

/*
Cross-DLL usage: These inline functions are used in ten_runtime
(ten_runtime.dll -> ten_utils.dll)

TEN_UTILS_API is used for DLL export on Windows. Without it, error
"undefined reference" will be raised.

1. Linux/macOS:
Global symbols declared with "extern" keyword are exported by default.

2. Windows(MSVC):
If the whole project is built with MSVC, then each DLL generates and uses
its own COMDAT copy of inline functions, eliminating the need for cross-DLL
imports. However, Go always uses MinGW toolchain on Windows (CGO requires
GCC-compatible compiler), regardless of whether the TEN runtime libraries
are built with MSVC or MinGW. So we need these exported implementations
whenever we're on Windows.
But MSVC does not export inline functions even with __declspec(dllexport).
So the header declares these as TEN_UTILS_API functions (not inline), and we
provide the definitions here.

3. Windows(MinGW):
According to GNU11 standard for inline functions, the "extern" keyword
should be used in .c file instead of .h file to prevent multiple definition.
So TEN_UTILS_API, which contains "extern" keyword, should be used here in .c
file.
Another solution:
MinGW uses GNU11 standard in this project, but we can use
__attribute__((gnu_inline)) in both .c and .h file to force the "inline"
keyword to work in GNU89 standard, which is exactly the opposite way.
("extern" keyword is used in .h file to prevent multiple definition)
And then TEN_UTILS_API can be used in .h file like the other functions.
*/

#if defined(_WIN32)

#if defined(_MSC_VER)
// Windows(MSVC)

TEN_RUNTIME_API ten_extension_t *ten_env_get_attached_extension(
    ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION,
             "Should not happen.");
  return self->attached_target.extension;
}

TEN_RUNTIME_API ten_extension_group_t *ten_env_get_attached_extension_group(
    ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_EXTENSION_GROUP,
             "Should not happen.");
  return self->attached_target.extension_group;
}

TEN_RUNTIME_API ten_app_t *ten_env_get_attached_app(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_APP, "Should not happen.");
  return self->attached_target.app;
}

TEN_RUNTIME_API ten_addon_host_t *ten_env_get_attached_addon(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON, "Should not happen.");
  return self->attached_target.addon_host;
}

TEN_RUNTIME_API ten_engine_t *ten_env_get_attached_engine(ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ENGINE, "Should not happen.");
  return self->attached_target.engine;
}

TEN_RUNTIME_API ten_addon_loader_t *ten_env_get_attached_addon_loader(
    ten_env_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(self, false), "Invalid use of ten_env %p.",
             self);
  TEN_ASSERT(self->attach_to == TEN_ENV_ATTACH_TO_ADDON_LOADER,
             "Should not happen.");
  return self->attached_target.addon_loader;
}

#else
// Windows(MinGW)
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

#endif

#else
// Linux/MacOS
extern inline ten_extension_t *ten_env_get_attached_extension(ten_env_t *self);

extern inline ten_extension_group_t *ten_env_get_attached_extension_group(
    ten_env_t *self);

extern inline ten_app_t *ten_env_get_attached_app(ten_env_t *self);

extern inline ten_addon_host_t *ten_env_get_attached_addon(ten_env_t *self);

extern inline ten_engine_t *ten_env_get_attached_engine(ten_env_t *self);

extern inline ten_addon_loader_t *ten_env_get_attached_addon_loader(
    ten_env_t *self);

#endif
