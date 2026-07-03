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

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/config.h"  // IWYU pragma: keep
#include "include_internal/ten_utils/backtrace/platform/posix/file.h"
#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "ten_utils/lib/atomic_ptr.h"
#include "ten_utils/macro/mark.h"

#if defined(HAVE_MACH_O_DYLD_H)
#include <mach-o/dyld.h>

static char *macho_get_executable_path(ten_backtrace_t *self,
                                       ten_backtrace_on_error_func_t on_error,
                                       void *data) {
  uint32_t len = 0;
  if (_NSGetExecutablePath(NULL, &len) == 0) {
    return NULL;
  }

  char *name = (char *)malloc(len);
  assert(name && "Failed to allocate memory.");

  if (_NSGetExecutablePath(name, &len) != 0) {
    free(name);
    return NULL;
  }

  return name;
}

#else /* !defined (HAVE_MACH_O_DYLD_H) */

#define macho_get_executable_path(self, on_error, data) NULL

#endif /* !defined (HAVE_MACH_O_DYLD_H) */

/**
 * @brief Initialize the file_line information from the executable.
 *
 * This function attempts to locate and open the current executable file
 * using various platform-specific methods:
 * 1. First tries "/proc/self/exe" (Linux)
 * 2. Then tries "/proc/curproc/file" (FreeBSD)
 * 3. Finally tries Mach-O specific method (macOS)
 *
 * Once the executable is found, it initializes the file/line lookup mechanism
 * for stack trace symbolization.
 *
 * @param self The backtrace context
 * @param on_error Callback function for reporting errors
 * @param data User data to pass to the error callback
 * @return true on success, false on failure
 */
static bool initialize_file_line_mechanism(
    ten_backtrace_t *self, ten_backtrace_on_error_func_t on_error, void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  // Check if initialization has already failed.
  int64_t failed = ten_atomic_load(&self_posix->file_line_init_failed);
  if (failed) {
    (void)fprintf(stderr, "Failed to read executable information.\n");
    return false;
  }

  // Check if already initialized successfully.
  ten_backtrace_on_get_file_line_func_t on_get_file_line =
      (ten_backtrace_on_get_file_line_func_t)ten_atomic_ptr_load(
          (ten_atomic_ptr_t *)&self_posix->on_get_file_line);
  if (on_get_file_line != NULL) {
    return true;
  }

  // We have not initialized the information. Do it now.

  int descriptor = -1;
  bool called_error_callback = false;
  const char *filename = NULL;

  // For storing dynamically allocated filename.
  char *dynamic_filename = NULL;

  // Try different methods to find the executable path.
  for (size_t pass = 0; pass < 3; ++pass) {
    switch (pass) {
    case 0:
      // Linux-specific method.
      filename = "/proc/self/exe";
      break;

    case 1:
      // FreeBSD-specific method.
      filename = "/proc/curproc/file";
      break;

    case 2:
      // macOS-specific method.
      // Free any previous dynamic filename.
      free(dynamic_filename);
      dynamic_filename = NULL;

      dynamic_filename = macho_get_executable_path(self, on_error, data);
      filename = dynamic_filename;
      break;

    default:
      abort();
      break;
    }

    if (filename == NULL) {
      // Failed to get the executable filename, try next method.
      continue;
    }

    bool does_not_exist = false;
    descriptor = ten_backtrace_open_file(filename, &does_not_exist);
    if (descriptor < 0 && !does_not_exist) {
      called_error_callback = true;
      break;
    }

    if (descriptor >= 0) {
      // Successfully opened the executable file.
      break;
    }

    // Failed to open the executable filename, try next method.
  }

  if (descriptor < 0) {
    if (!called_error_callback) {
      on_error(data, "Failed to find executable to open.", 0, NULL);
    }
    failed = 1;
  }

  if (!failed) {
    if (!ten_backtrace_init_posix(self, filename, descriptor, on_error, data,
                                  &on_get_file_line)) {
      failed = 1;
    }
  }

  // Free dynamic_filename after we're done with it.
  if (dynamic_filename != NULL) {
    free(dynamic_filename);
    dynamic_filename = NULL;
  }

  if (failed) {
    ten_atomic_store(&self_posix->file_line_init_failed, 1);
    return false;
  }

  // Store the function pointer atomically to ensure thread safety.
  //
  // TODO(Wei): Note that if two threads initialize at once, one of the data
  // sets may be leaked. Might need to consider a new way to avoid this.
  ten_atomic_ptr_store((ten_atomic_ptr_t *)&self_posix->on_get_file_line,
                       on_get_file_line);

  return true;
}

/**
 * @brief Given a PC, find the file name, line number, and function name.
 */
int ten_backtrace_get_file_line_info(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error, void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  if (!initialize_file_line_mechanism(self, on_error, data)) {
    return 0;
  }

  if (ten_atomic_load(&self_posix->file_line_init_failed)) {
    return 0;
  }

  return (self_posix->on_get_file_line)(self, pc, on_dump_file_line, on_error,
                                        data);
}

/**
 * @brief Given a PC, find the symbol for it, and its value.
 */
int ten_backtrace_get_syminfo(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
    ten_backtrace_on_error_func_t on_error, void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  if (!initialize_file_line_mechanism(self, on_error, data)) {
    return 0;
  }

  if (ten_atomic_load(&self_posix->file_line_init_failed)) {
    return 0;
  }

  self_posix->on_get_syminfo(self, pc, on_dump_syminfo, on_error, data);

  return 1;
}

/**
 * @brief Adapter function that converts symbol information to file/line format.
 *
 * This function is used as a callback for symbol table lookups when we have a
 * symbol table but no debug information. It adapts the symbol information
 * (function name) to the file/line callback format, passing NULL for the
 * filename and 0 for the line number since this information is not available
 * from symbol tables alone.
 *
 * @param self The backtrace context.
 * @param pc The program counter address being looked up.
 * @param symname The symbol name found for this address (function name).
 * @param sym_val The symbol value (address), unused in this adapter.
 * @param sym_size The symbol size, unused in this adapter.
 * @param data Pointer to a backtrace_call_full structure containing the
 * original callbacks.
 */
void backtrace_dump_syminfo_to_file_line(ten_backtrace_t *self, uintptr_t pc,
                                         const char *symname,
                                         TEN_UNUSED uintptr_t sym_val,
                                         TEN_UNUSED uintptr_t sym_size,
                                         void *data) {
  backtrace_call_full *bt_data = (backtrace_call_full *)data;
  assert(bt_data && "Invalid argument.");

  // Call the file/line callback with the program counter and symbol name,
  // but with NULL filename and 0 line number since we don't have that
  // information.
  bt_data->ret =
      bt_data->on_dump_file_line(self, pc, NULL, 0, symname, bt_data->data);
}

/**
 * @brief Error callback adapter for symbol-to-file/line conversion.
 *
 * This function serves as an error callback adapter when converting symbol
 * information to file/line format. It forwards error messages from symbol
 * lookup operations to the original error callback stored in the
 * `backtrace_call_full` structure.
 *
 * This function is paired with backtrace_dump_syminfo_to_file_line() and
 * is used when we have symbol information but no debug information, allowing
 * error handling to be properly propagated through the callback chain.
 *
 * @param self The backtrace context.
 * @param msg The error message to report.
 * @param errnum The error number (errno value) or -1 if not applicable.
 * @param data Pointer to a backtrace_call_full structure containing the
 *             original error callback and its user data.
 */
void backtrace_dump_syminfo_to_file_line_error(ten_backtrace_t *self,
                                               const char *msg, int errnum,
                                               void *data) {
  backtrace_call_full *bt_data = (backtrace_call_full *)data;
  assert(bt_data && "Invalid argument.");

  // Forward the error to the original error callback.
  bt_data->on_error(self, msg, errnum, bt_data->data);
}
