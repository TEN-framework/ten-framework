//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <Windows.h>
#include <assert.h>
#include <dbghelp.h>
#include <inttypes.h>
#include <stddef.h>

#include "include_internal/ten_utils/backtrace/common.h"

/**
 * @brief Function type for SymInitialize from DbgHelp.dll
 */
typedef BOOL(WINAPI *win_SymInitialize_func_t)(HANDLE hProcess,
                                               PCSTR UserSearchPath,
                                               BOOL fInvadeProcess);

/**
 * @brief Function type for SymCleanup from DbgHelp.dll
 */
typedef BOOL(WINAPI *win_SymCleanup_func_t)(HANDLE hProcess);

/**
 * @brief Function type for SymGetOptions from DbgHelp.dll
 */
typedef DWORD(WINAPI *win_SymGetOptions_func_t)(VOID);

/**
 * @brief Function type for SymSetOptions from DbgHelp.dll
 */
typedef DWORD(WINAPI *win_SymSetOptions_func_t)(DWORD);

/**
 * @brief Function type for SymFromAddr from DbgHelp.dll
 */
typedef BOOL(WINAPI *win_SymFromAddr_func_t)(HANDLE hProcess, DWORD64 Address,
                                             PDWORD64 Displacement,
                                             PSYMBOL_INFO Symbol);

/**
 * @brief Function type for SymGetLineFromAddr from DbgHelp.dll
 */
typedef BOOL(WINAPI *win_SymGetLineFromAddr_func_t)(HANDLE hProcess,
                                                    DWORD dwAddr,
                                                    PDWORD pdwDisplacement,
                                                    PIMAGEHLP_LINE Line);

/**
 * @brief Function type for RtlCaptureStackBackTrace from NtDll.dll
 */
typedef WORD(WINAPI *win_RtlCaptureStackBackTrace_func_t)(DWORD FramesToSkip,
                                                          DWORD FramesToCapture,
                                                          PVOID *BackTrace,
                                                          PDWORD BackTraceHash);

/**
 * @brief The Windows specific ten_backtrace_t implementation.
 */
typedef struct ten_backtrace_win_t {
  ten_backtrace_common_t common;

  // DLL handles for cleanup.
  HMODULE dbghelp_handle;
  HMODULE ntdll_handle;

  // @{
  // The Windows functions used to dump backtrace.

  // From NtDll.dll
  win_RtlCaptureStackBackTrace_func_t RtlCaptureStackBackTrace;

  // From DbgHelp.dll
  win_SymInitialize_func_t SymInitialize;
  win_SymCleanup_func_t SymCleanup;
  win_SymGetOptions_func_t SymGetOptions;
  win_SymSetOptions_func_t SymSetOptions;
  win_SymFromAddr_func_t SymFromAddr;
  win_SymGetLineFromAddr_func_t SymGetLineFromAddr;
  // @}
} ten_backtrace_win_t;
