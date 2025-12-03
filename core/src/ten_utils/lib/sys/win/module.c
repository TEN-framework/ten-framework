//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/module.h"

#include <Windows.h>
#include <stdio.h>
#include <string.h>

#include "ten_utils/log/log.h"

// Windows long path support: maximum path length is 32767 characters.
// This is much larger than the traditional MAX_PATH (260 characters).
#define TEN_MAX_PATH 32767

// Helper function to add a DLL directory to the search path.
static PVOID add_dll_directory_helper(const char *dir_path) {
  if (!dir_path || dir_path[0] == '\0') {
    TEN_LOGE("add_dll_directory: NULL or empty path");
    return NULL;
  }
  TEN_LOGI("add_dll_directory: %s", dir_path);

  // Convert the multi-byte path string to wide character string (UTF-16).
  // This is required because Windows Unicode API functions (like
  // AddDllDirectory) require wide character strings.
  //
  // We try UTF-8 encoding first (CP_UTF8), which is the standard encoding used
  // in TEN framework. If the conversion fails (e.g., the input string is not
  // valid UTF-8), we fall back to the system's default ANSI code page (CP_ACP)
  // for compatibility.
  //
  // This dual-encoding approach ensures we can handle paths with:
  // - UTF-8 encoded strings (from TEN framework internal usage)
  // - ANSI encoded strings (from legacy systems or external sources)
  // - Non-ASCII characters (e.g., Chinese, Japanese paths)
  wchar_t dir_w[TEN_MAX_PATH];
  int wlen = MultiByteToWideChar(CP_UTF8, 0, dir_path, -1, dir_w, TEN_MAX_PATH);
  if (wlen == 0) {
    wlen = MultiByteToWideChar(CP_ACP, 0, dir_path, -1, dir_w, TEN_MAX_PATH);
  }

  if (wlen > 0) {
    PVOID cookie = AddDllDirectory(dir_w);
    if (!cookie) {
      TEN_LOGE("AddDllDirectory failed for path: %s", dir_path);
    }
    return cookie;
  } else {
    TEN_LOGE("MultiByteToWideChar failed for path: %s", dir_path);
    return NULL;
  }
}

// Helper function to find Python 3.10 installation directory by searching for
// python310.dll in the PATH environment variable and common installation paths.
static int find_python_dll_directory(char *output_path, size_t output_size) {
  if (!output_path || output_size == 0) {
    return 0;
  }

  // TEN Framework officially supports Python 3.10.
  // See: .github/workflows/*.yml files where python-version is set to "3.10"

  // Method 1: Search in PATH environment variable (fastest if Python is in
  // PATH)
  // This is the quickest way to find Python if the user installed it with
  // "Add Python to PATH" option checked.
  char path_env[TEN_MAX_PATH * 4];  // PATH can be very long
  DWORD path_len = GetEnvironmentVariableA("PATH", path_env, sizeof(path_env));
  if (path_len > 0 && path_len < sizeof(path_env)) {
    char *next_token = NULL;
    char *token = strtok_s(path_env, ";", &next_token);
    while (token != NULL) {
      char test_path[TEN_MAX_PATH];
      (void)snprintf(test_path, TEN_MAX_PATH, "%s\\python310.dll", token);

      // Check if python310.dll exists in this directory
      DWORD attrib = GetFileAttributesA(test_path);
      if (attrib != INVALID_FILE_ATTRIBUTES &&
          !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        // Found python310.dll
        (void)snprintf(output_path, output_size, "%s", token);
        TEN_LOGI("Found Python 3.10 directory in PATH: %s", output_path);
        return 1;
      }
      token = strtok_s(NULL, ";", &next_token);
    }
  }

  // Method 2: Check user's local Python 3.10 installation
  // This is the default installation path when using the official Python
  // installer with "Install for current user only" option (which is the
  // default). Example: C:\Users\XXX\AppData\Local\Programs\Python\Python310
  //
  // According to Python's official installer behavior, this covers 95%+ of
  // user installations. We deliberately skip checking Program Files because:
  // - Python installer defaults to per-user installation (LOCALAPPDATA)
  // - System-wide installation requires admin privileges and is rarely used
  // - Keeps the search fast and focused on the most common scenario
  char local_appdata[TEN_MAX_PATH];
  DWORD local_len =
      GetEnvironmentVariableA("LOCALAPPDATA", local_appdata, TEN_MAX_PATH);
  if (local_len > 0) {
    char test_path[TEN_MAX_PATH];
    (void)snprintf(test_path, TEN_MAX_PATH,
                   "%s\\Programs\\Python\\Python310\\python310.dll",
                   local_appdata);

    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES &&
        !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
      // Found python310.dll in user's local Python installation
      (void)snprintf(output_path, output_size,
                     "%s\\Programs\\Python\\Python310", local_appdata);
      TEN_LOGI("Found Python 3.10 directory: %s", output_path);
      return 1;
    }
  }

  TEN_LOGE(
      "Could not find Python 3.10 installation. Please ensure Python 3.10 is "
      "installed and either added to PATH or installed in the default "
      "location.");
  return 0;
}

void *ten_module_load(const ten_string_t *name, int as_local) {
  (void)as_local;
  if (!name || ten_string_is_empty(name)) {
    return NULL;
  }

  const char *dll_path = ten_string_get_raw_str(name);
  TEN_LOGI("ten_module_load: attempting to load '%s'", dll_path);

  // ==============================================================================
  // Add dependency DLL directories to the search path for
  // python_addon_loader.dll
  // ==============================================================================
  // For python_addon_loader.dll, add dependency DLL directories to enable it
  // to find ten_runtime.dll, ten_utils.dll, ten_runtime_python.dll and
  // python310.dll.

  // This is only needed when a cpp app with Python extensions is running on
  // Windows platform:
  // 1. Unix-like platforms (Linux, macOS) use rpath to embed dependency search
  //    paths in the shared library itself at build time:
  //    - Linux uses: -Wl,-rpath=$ORIGIN and ch=$ORIGIN/../lib/
  //    - macOS uses: -Wl,-rpath,@loader_path and
  //    -Wl,-rpath,@loader_path/../lib/ This allows the dynamic linker to
  //    automatically find dependencies.
  // 2. Windows does not support rpath. Instead, it uses DLL search order:
  //    - The directory containing the executable
  //    - System directories (System32, etc.)
  //    - The current working directory
  //    - Directories in PATH environment variable
  //    Since python_addon_loader.dll's dependencies (ten_runtime.dll,
  //    ten_utils.dll, ten_runtime_python.dll, python310.dll) are NOT in these
  //    default search paths, we must explicitly add their directories using
  //    AddDllDirectory.
  // 3. For Python applications on Windows:
  //    - Python apps load libten_runtime_python.pyd when executing
  //      "from ten_runtime import ..." in their main.py
  //    - This also loads its dependencies (ten_runtime.dll, ten_utils.dll)
  //    - For python apps, DLL search paths are configured in
  //      ten_runtime/__init__.py (see binding/python/interface/)
  //    - For C++ apps with Python extensions, we handle it here instead
  PVOID ten_runtime_cookie = NULL;
  PVOID ten_runtime_python_cookie = NULL;
  PVOID python_installation_cookie = NULL;
  if (strstr(dll_path, "python_addon_loader") != NULL) {
    // Calculate directory path length (up to the last path separator)
    const char *sep = strrchr(dll_path, '\\');
    if (!sep) sep = strrchr(dll_path, '/');

    if (sep) {
      int dir_len = (int)(sep - dll_path);

      // Path structure is fixed:
      // app_base_dir/ten_packages/addon_loader/python_addon_loader/lib/python_addon_loader.dll
      // Go up 4 levels from dll_dir to get app_base_dir
      char app_base_dir[TEN_MAX_PATH];
      (void)snprintf(app_base_dir, TEN_MAX_PATH, "%.*s\\..\\..\\..\\..",
                     dir_len, dll_path);
      char normalized_app_base_dir[TEN_MAX_PATH];
      GetFullPathNameA(app_base_dir, TEN_MAX_PATH, normalized_app_base_dir,
                       NULL);

      // Because python_addon_loader.dll depends on ten_runtime.dll and
      // ten_runtime_python.dll in
      // app_base_dir/ten_packages/system/ten_runtime/lib.
      char ten_runtime_lib[TEN_MAX_PATH];
      (void)snprintf(ten_runtime_lib, TEN_MAX_PATH,
                     "%s\\ten_packages\\system\\ten_runtime\\lib",
                     normalized_app_base_dir);
      ten_runtime_cookie = add_dll_directory_helper(ten_runtime_lib);

      // Because python_addon_loader.dll depends on ten_runtime_python.dll
      // in app_base_dir/ten_packages/system/ten_runtime_python/lib.
      char ten_runtime_python_lib[TEN_MAX_PATH];
      (void)snprintf(ten_runtime_python_lib, TEN_MAX_PATH,
                     "%s\\ten_packages\\system\\ten_runtime_python\\lib",
                     normalized_app_base_dir);
      ten_runtime_python_cookie =
          add_dll_directory_helper(ten_runtime_python_lib);

      // Because python_addon_loader.dll depends on python310.dll from the
      // Python 3.10 installation directory. We need to find and add it to the
      // search path.
      char python_dir[TEN_MAX_PATH];
      if (find_python_dll_directory(python_dir, TEN_MAX_PATH)) {
        python_installation_cookie = add_dll_directory_helper(python_dir);
      }
    } else {
      TEN_LOGE("Failed to get separator from DLL path: %s", dll_path);
    }
  }
  // =============================================================================

  // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS:
  // represents the recommended maximum number of directories an application
  // should include in its DLL search path. (a combination of application dir,
  // system32 dir, and user dir which is affected by AddDllDirectory function)

  // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: the directory that contains the DLL is
  // temporarily added to the beginning of the list of directories that are
  // searched for the DLL's dependencies.

  // Each argument will cause directories in the standard search paths not to be
  // searched.
  HMODULE loaded_module = LoadLibraryExA(
      ten_string_get_raw_str(name), NULL,
      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);

  if (!loaded_module) {
    DWORD error_code = GetLastError();
    TEN_LOGE("Failed to load DLL '%s' with error %lu", dll_path, error_code);
  } else {
    TEN_LOGI("Successfully loaded DLL '%s' at address %p", dll_path,
             (void *)loaded_module);
  }

  // Remove the temporary directories to prevent memory leak
  if (python_installation_cookie) {
    RemoveDllDirectory(python_installation_cookie);
    TEN_LOGI("Removed Python installation DLL directory from search path");
  }
  if (ten_runtime_python_cookie) {
    RemoveDllDirectory(ten_runtime_python_cookie);
    TEN_LOGI("Removed ten_runtime_python DLL directory from search path");
  }
  if (ten_runtime_cookie) {
    RemoveDllDirectory(ten_runtime_cookie);
    TEN_LOGI("Removed ten_runtime DLL directory from search path");
  }

  return (void *)loaded_module;
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
