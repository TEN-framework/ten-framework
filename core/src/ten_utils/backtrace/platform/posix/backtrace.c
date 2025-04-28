//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/backtrace.h"

#include <assert.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "unwind.h"

/**
 * @brief Data passed through _Unwind_Backtrace.
 */
typedef struct backtrace_data {
  size_t skip;  // Number of frames to skip.
  ten_backtrace_t *ten_backtrace;
  int ret;  // Value to return from ten_backtrace_dump_using_libgcc.
} backtrace_data;

/**
 * @brief Unwind library callback routine. This is passed to _Unwind_Backtrace.
 */
static _Unwind_Reason_Code unwind(struct _Unwind_Context *context, void *data) {
  backtrace_data *bt_data = (backtrace_data *)data;
  int ip_before_insn = 0;

  // _Unwind_GetIPInfo() returns the instruction pointer for the current frame,
  // and this function is normally thread-safe.
  uintptr_t pc = _Unwind_GetIPInfo(context, &ip_before_insn);

  if (bt_data->skip > 0) {
    --bt_data->skip;
    return _URC_NO_REASON;
  }

  if (!ip_before_insn) {
    --pc;
  }

  bt_data->ret = ten_backtrace_get_file_line_info(
      bt_data->ten_backtrace, pc,
      ((ten_backtrace_common_t *)bt_data->ten_backtrace)->on_dump_file_line,
      ((ten_backtrace_common_t *)bt_data->ten_backtrace)->on_error,
      ((ten_backtrace_common_t *)bt_data->ten_backtrace)->cb_data);
  if (bt_data->ret != 0) {
    return _URC_END_OF_STACK;
  }

  return _URC_NO_REASON;
}

/**
 * @brief Captures and dumps a stack backtrace using libgcc's unwinder.
 *
 * This function uses libgcc's _Unwind_Backtrace to capture the current call
 * stack and process it. The unwinder can provide more detailed file and line
 * information when debug symbols are available, which can be more informative
 * than the basic backtrace provided by glibc.
 *
 * @param self Pointer to the backtrace object. Must not be NULL.
 * @param skip Number of stack frames to skip from the top of the call stack.
 *             This is useful to exclude the backtrace function itself and
 *             its immediate callers from the output.
 *
 * @return 0 on success, non-zero on failure.
 */
int ten_backtrace_dump_using_libgcc(ten_backtrace_t *self, size_t skip) {
  assert(self && "Invalid argument.");

  (void)dprintf(STDERR_FILENO, "======= Backtrace using libgcc =======\n");

  backtrace_data bt_data;

  // +1 is to skip the '_Unwind_Backtrace' function itself.
  bt_data.skip = skip + 1;
  bt_data.ten_backtrace = self;
  bt_data.ret = 0;

  // _Unwind_Backtrace() performs a stack backtrace using unwind data.
  // This function is thread-safe and passes each frame to the unwind callback.
  _Unwind_Backtrace(unwind, &bt_data);

  return bt_data.ret;
}

/**
 * @brief Get a stack backtrace.
 */
int ten_backtrace_dump_using_glibc(ten_backtrace_t *self, size_t skip) {
  assert(self && "Invalid argument.");

  void *call_stack[MAX_CAPTURED_CALL_STACK_DEPTH];

  // Capture backtrace.
  int frame_size = backtrace(call_stack, MAX_CAPTURED_CALL_STACK_DEPTH);
  if (frame_size <= 0) {
    (void)dprintf(STDERR_FILENO, "Failed to get backtrace using glibc\n");

    ten_backtrace_common_t *self_common = (ten_backtrace_common_t *)self;
    self_common->on_error(self, "Failed to capture backtrace", -1,
                          self_common->cb_data);
    return -1;
  }

  char **symbols = backtrace_symbols(call_stack, frame_size);
  if (!symbols) {
    (void)dprintf(STDERR_FILENO,
                  "Failed to get backtrace symbols using glibc\n");

    ten_backtrace_common_t *self_common = (ten_backtrace_common_t *)self;
    self_common->on_error(self, "Failed to get backtrace symbols", -1,
                          self_common->cb_data);
    return -1;
  }

  // +2 is to skip `ten_backtrace_dump` and `ten_backtrace_dump_using_glibc`.
  skip = skip + 2;

  // Ensure skip doesn't exceed the number of frames.
  skip = (skip < frame_size) ? skip : frame_size;

  // Print backtrace to log.
  (void)dprintf(STDERR_FILENO,
                "======= Backtrace using glibc (%zu frames) =======\n",
                frame_size - skip);

  for (size_t i = skip; i < frame_size; i++) {
    (void)dprintf(STDERR_FILENO, "#%zu: %s\n", i - skip, symbols[i]);
  }

  free((void *)symbols);

  // For high reliability, we can also dump directly to file descriptor.
  (void)dprintf(STDERR_FILENO, "======= Raw backtrace using glibc =======\n");
  backtrace_symbols_fd(call_stack, frame_size, STDERR_FILENO);

  return 0;
}
