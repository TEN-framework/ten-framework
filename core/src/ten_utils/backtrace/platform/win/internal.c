//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/win/internal.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/common.h"

/**
 * @brief Dynamically loads and retrieves function pointers for Windows
 * backtrace functionality.
 *
 * This function loads the necessary DLLs (DbgHelp.dll and NtDll.dll) and
 * retrieves function pointers for symbol handling and stack trace capture.
 * It asserts if any required function cannot be found.
 *
 * @param self Pointer to the Windows backtrace structure. Must not be NULL.
 */
static void retrieve_windows_backtrace_funcs(ten_backtrace_win_t *self) {
  assert(self && "Invalid argument.");

  self->dbghelp_handle = LoadLibraryA("DbgHelp.dll");
  if (self->dbghelp_handle != NULL) {
    self->SymFromAddr = (win_SymFromAddr_func_t)GetProcAddress(
        self->dbghelp_handle, "SymFromAddr");
    if (!self->SymFromAddr) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymFromAddr\n");
      assert(0 && "Failed to find DbgHelp.SymFromAddr");
    }

    self->SymGetLineFromAddr = (win_SymGetLineFromAddr_func_t)GetProcAddress(
        self->dbghelp_handle, "SymGetLineFromAddr64");
    if (!self->SymGetLineFromAddr) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymGetLineFromAddr\n");
      assert(0 && "Failed to find DbgHelp.SymGetLineFromAddr");
    }

    self->SymInitialize = (win_SymInitialize_func_t)GetProcAddress(
        self->dbghelp_handle, "SymInitialize");
    if (!self->SymInitialize) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymInitialize\n");
      assert(0 && "Failed to find DbgHelp.SymInitialize");
    }

    self->SymCleanup = (win_SymCleanup_func_t)GetProcAddress(
        self->dbghelp_handle, "SymCleanup");
    if (!self->SymCleanup) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymCleanup\n");
      assert(0 && "Failed to find DbgHelp.SymCleanup");
    }

    self->SymGetOptions = (win_SymGetOptions_func_t)GetProcAddress(
        self->dbghelp_handle, "SymGetOptions");
    if (!self->SymGetOptions) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymGetOptions\n");
      assert(0 && "Failed to find DbgHelp.SymGetOptions");
    }

    self->SymSetOptions = (win_SymSetOptions_func_t)GetProcAddress(
        self->dbghelp_handle, "SymSetOptions");
    if (!self->SymSetOptions) {
      fprintf(stderr, "Warning: Failed to find DbgHelp.SymSetOptions\n");
      assert(0 && "Failed to find DbgHelp.SymSetOptions");
    }
  } else {
    fprintf(stderr, "Warning: Failed to load DbgHelp.dll\n");
    assert(0 && "Failed to load DbgHelp.dll");
  }

  self->ntdll_handle = LoadLibraryA("NtDll.dll");
  if (self->ntdll_handle != NULL) {
    self->RtlCaptureStackBackTrace =
        (win_RtlCaptureStackBackTrace_func_t)GetProcAddress(
            self->ntdll_handle, "RtlCaptureStackBackTrace");
    if (!self->RtlCaptureStackBackTrace) {
      fprintf(stderr,
              "Warning: Failed to find NtDll.RtlCaptureStackBackTrace\n");
      assert(0 && "Failed to find NtDll.RtlCaptureStackBackTrace");
    }
  } else {
    fprintf(stderr, "Warning: Failed to load NtDll.dll\n");
    assert(0 && "Failed to load NtDll.dll");
  }
}

/**
 * @brief Creates a new backtrace object for Windows platform.
 *
 * This function allocates memory for a `ten_backtrace_win_t` structure and
 * initializes its fields with default values. It sets up the common fields
 * with default callback functions for dumping stack traces and handling errors,
 * and loads the necessary Windows functions for backtrace functionality.
 *
 * @return A pointer to the newly created backtrace object cast to
 * `ten_backtrace_t`, or `NULL` if memory allocation fails.
 *
 * @note The returned object must be freed with `ten_backtrace_destroy()` when
 * no longer needed.
 */
ten_backtrace_t *ten_backtrace_create(void) {
  ten_backtrace_win_t *self = calloc(1, sizeof(ten_backtrace_win_t));
  if (!self) {
    fprintf(stderr, "Error: Failed to allocate memory for backtrace.\n");
    assert(0 && "Failed to allocate memory.");
    return NULL;
  }

  ten_backtrace_common_init(&self->common, ten_backtrace_default_dump,
                            ten_backtrace_default_error);
  retrieve_windows_backtrace_funcs(self);

  return (ten_backtrace_t *)self;
}

/**
 * @brief Destroys a backtrace object and frees associated resources.
 *
 * This function properly cleans up resources associated with the backtrace
 * object by first calling the common deinitialization function and then
 * freeing the memory allocated for the object itself.
 *
 * @param self Pointer to the backtrace object to destroy. Must not be NULL.
 */
void ten_backtrace_destroy(ten_backtrace_t *self) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return;
  }

  ten_backtrace_win_t *self_win = (ten_backtrace_win_t *)self;

  // Clean up DLL handles
  if (self_win->dbghelp_handle) {
    FreeLibrary(self_win->dbghelp_handle);
  }

  if (self_win->ntdll_handle) {
    FreeLibrary(self_win->ntdll_handle);
  }

  ten_backtrace_common_deinit(self);
  free(self);
}

/**
 * @brief Dumps the current call stack.
 *
 * This function captures the current call stack using Windows-specific APIs
 * and processes it to retrieve symbol and line information. It then calls
 * the registered dump callback function for each frame in the stack trace.
 *
 * @param self Pointer to the backtrace object. Must not be NULL.
 * @param skip Number of stack frames to skip from the top of the call stack.
 *             This is useful to exclude the backtrace function itself and
 *             its immediate callers from the output.
 */
void ten_backtrace_dump(ten_backtrace_t *self, size_t skip) {
  if (!self) {
    fprintf(stderr, "Error: Invalid backtrace object.\n");
    assert(0 && "Invalid argument.");
    return;
  }

  ten_backtrace_win_t *self_win = (ten_backtrace_win_t *)self;

  // Check if we have all required function pointers.
  if (!self_win->SymInitialize || !self_win->SymCleanup ||
      !self_win->SymGetOptions || !self_win->SymSetOptions ||
      !self_win->SymFromAddr || !self_win->SymGetLineFromAddr ||
      !self_win->RtlCaptureStackBackTrace) {
    fprintf(stderr, "Missing required Windows backtrace functions.\n");
    return;
  }

  HANDLE process = GetCurrentProcess();

  // Configure symbol handler options.
  self_win->SymSetOptions(self_win->SymGetOptions() | SYMOPT_LOAD_LINES |
                          SYMOPT_DEFERRED_LOADS);

  // Initialize symbol handler.
  if (!self_win->SymInitialize(process, NULL, TRUE)) {
    fprintf(stderr, "Failed to initialize symbol handler: %lu\n",
            GetLastError());
    return;
  }

  // Capture the stack trace.
  void *stack[MAX_CAPTURED_CALL_STACK_DEPTH] = {0};

  USHORT frames = self_win->RtlCaptureStackBackTrace(
      0, MAX_CAPTURED_CALL_STACK_DEPTH, stack, NULL);

  if (frames == 0) {
    fprintf(stderr, "Warning: No stack frames captured.\n");
    self_win->SymCleanup(process);
    return;
  }

  if (skip >= frames) {
    fprintf(stderr,
            "Warning: Skip count (%zu) exceeds available frames (%hu).\n", skip,
            frames);
    self_win->SymCleanup(process);
    return;
  }

  const size_t MAX_SYMBOL_NAME_LEN = 1024;

  SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(
      offsetof(SYMBOL_INFO, Name) + MAX_SYMBOL_NAME_LEN * sizeof(char), 1);
  if (!symbol) {
    fprintf(stderr, "Failed to allocate memory for symbol information.\n");
    assert(0 && "Failed to allocate memory.");
    self_win->SymCleanup(process);
    return;
  }

  symbol->MaxNameLen = MAX_SYMBOL_NAME_LEN - 1;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  // Process each frame in the stack trace.
  for (size_t i = skip; i < frames; i++) {
    DWORD64 address = (DWORD64)(stack[i]);

    // Get symbol information.
    if (!self_win->SymFromAddr(process, address, 0, symbol)) {
      fprintf(stderr, "Warning: Failed to get symbol for address 0x%llx: %lu\n",
              (unsigned long long)address, GetLastError());
      continue;
    }

    // Get line information.
    IMAGEHLP_LINE lineInfo = {0};
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE);
    DWORD dwLineDisplacement = 0;

    if (self_win->SymGetLineFromAddr(process, address, &dwLineDisplacement,
                                     &lineInfo)) {
      // Call the dump callback with full information.
      self_win->common.on_dump_file_line(self, address, lineInfo.FileName,
                                         lineInfo.LineNumber, symbol->Name,
                                         NULL);
    } else {
      // Call the dump callback with only symbol information.
      self_win->common.on_dump_file_line(self, address, NULL, 0, symbol->Name,
                                         NULL);
    }
  }

  // Clean up.
  free(symbol);
  self_win->SymCleanup(process);
}
