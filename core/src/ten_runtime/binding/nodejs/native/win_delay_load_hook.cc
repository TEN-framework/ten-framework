/*
 * Copyright © 2025 Agora
 * This file is part of TEN Framework, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 *
 *
 * Ref: https://github.com/nodejs/node-gyp/blob/main/src/win_delay_load_hook.cc
 *
 * When this file is linked to a DLL, it sets up a delay-load hook that
 * intervenes when the DLL is trying to load libnode.dll dynamically.
 *
 * For pure Node.js apps (started via node.exe), the N-API symbols are
 * provided by node.exe itself, so we return a handle to the current process.
 *
 * For embedded Node.js scenarios (C++/Go apps with nodejs_addon_loader),
 * libnode.dll is loaded separately, so we try to get its handle first.
 *
 * This allows the addon to work in both scenarios without requiring
 * libnode.dll to be present for pure Node.js apps.
 */

#ifdef _MSC_VER

#pragma managed(push, off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <delayimp.h>
#include <string.h>

static FARPROC WINAPI load_exe_hook(unsigned int event, DelayLoadInfo* info) {
  HMODULE m;

  if (event != dliNotePreLoadLibrary) {
    return NULL;
  }

  // Check if the DLL being loaded is libnode.dll
  if (_stricmp(info->szDll, "libnode.dll") != 0) {
    return NULL;
  }

  // First, try to get libnode.dll if it's already loaded
  // (for embedded Node.js scenarios like C++/Go apps with nodejs_addon_loader)
  m = GetModuleHandle(TEXT("libnode.dll"));

  if (m == NULL) {
    // libnode.dll is not loaded, we're running in a pure Node.js app
    // where node.exe provides the N-API symbols directly.
    // Return the handle to the current process (node.exe).
    m = GetModuleHandle(NULL);
  }

  return (FARPROC)m;
}

// Register the delay-load hook.
// This symbol overrides the default delay-load helper in delayimp.lib.
decltype(__pfnDliNotifyHook2) __pfnDliNotifyHook2 = load_exe_hook;

#pragma managed(pop)

#endif
