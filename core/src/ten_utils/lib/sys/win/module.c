//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/module.h"

#include <Windows.h>

#include "ten_utils/log/log.h"

void *ten_module_load(const ten_string_t *name, int as_local) {
  (void)as_local;
  if (!name || ten_string_is_empty(name)) {
    return NULL;
  }

  // Use standard LoadLibrary behavior (flags = 0) to allow DLL search via PATH.
  //
  // The special flags LOAD_LIBRARY_SEARCH_DEFAULT_DIRS and
  // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR are intentionally NOT used because:
  //
  // 1. LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: Only searches the DLL's own directory
  //    for dependencies, completely ignoring PATH. This requires copying all
  //    dependency DLLs (ten_runtime.dll, ten_utils.dll, ten_runtime_python.dll,
  //    pythonXX.dll) to each addon_loader directory, which is cumbersome.
  //
  // 2. LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: Searches system directories (System32,
  //    Windows) but still ignores the PATH environment variable. This value is
  //    a combination of LOAD_LIBRARY_SEARCH_APPLICATION_DIR,
  //     LOAD_LIBRARY_SEARCH_SYSTEM32, and LOAD_LIBRARY_SEARCH_USER_DIRS.
  //
  // By using flags = 0 (standard behavior), Windows searches for DLL
  // dependencies in the following order:
  //   - Application directory
  //   - System32, System, Windows directories
  //   - Current working directory
  //   - Directories in PATH environment variable  <-- This is what we want
  //
  // Trade-off: This sacrifices some isolation and may load unintended DLL
  // versions if PATH is misconfigured, giving chance to DLL hijacking, 
  // but it's much more convenient for development and deployment (just 
  // set PATH in start.py).
  //
  // Reference:
  // https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa
  // https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order
  return (void *)LoadLibraryExA(ten_string_get_raw_str(name), NULL, 0);
}

int ten_module_close(void *handle) {
  return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

void *ten_module_get_symbol(void *handle, const char *symbol_name) {
  if (!handle) {
    TEN_LOGE("Invalid argument: handle is null");
    return NULL;
  }

  if (!symbol_name) {
    TEN_LOGE("Invalid argument: symbol name is null or empty");
    return NULL;
  }

  FARPROC symbol = GetProcAddress((HMODULE)handle, symbol_name);
  if (!symbol) {
    DWORD error_code = GetLastError();
    LPVOID error_message = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR)&error_message, 0, NULL);

    // Enable the code below if debugging is needed.
#if 0
    TEN_LOGE("Failed to find symbol %s: %s", symbol_name,
             error_message ? (char *)error_message : "Unknown error");
#endif

    if (error_message) {
      LocalFree(error_message);
    }

    return NULL;
  }

  return (void *)symbol;
}
