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

  // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: the directory that contains the DLL is
  // temporarily added to the beginning of the list of directories that are
  // searched for the DLL's dependencies. Directories in the standard search
  // path are not searched.
  return (void *)LoadLibraryExA(
      ten_string_get_raw_str(name), NULL,
      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
}

int ten_module_close(void *handle) {
  return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

void *ten_module_get_symbol(void *handle, const char *symbol_name) {
  if (!handle) {
    TEN_LOGE("Invalid argument: handle is null.");
    return NULL;
  }

  if (!symbol_name) {
    TEN_LOGE("Invalid argument: symbol name is null or empty.");
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
