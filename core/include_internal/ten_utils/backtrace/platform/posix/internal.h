//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/common.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/macro/mark.h"

#ifndef __GNUC__
#define __builtin_prefetch(p, r, l)
#define unlikely(x) (x)
#else
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

typedef struct dwarf_sections dwarf_sections;
typedef struct dwarf_data dwarf_data;

/**
 * @brief The type of the function that collects file/line information.
 */
typedef int (*ten_backtrace_on_get_file_line_func_t)(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error, void *data);

/**
 * @brief The type of the function that collects symbol information.
 */
typedef void (*ten_backtrace_on_get_syminfo_func_t)(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
    ten_backtrace_on_error_func_t on_error, void *data);

/**
 * @brief The posix specific ten_backtrace_t implementation.
 */
typedef struct ten_backtrace_posix_t {
  ten_backtrace_common_t common;

  // The function that returns file/line information.
  ten_backtrace_on_get_file_line_func_t on_get_file_line;
  // The data to pass to 'on_get_file_line'.
  void *on_get_file_line_data;

  // The function that returns symbol information.
  ten_backtrace_on_get_syminfo_func_t on_get_syminfo;
  // The data to pass to 'on_get_syminfo'.
  void *on_get_syminfo_data;

  // Whether initializing the file/line information failed.
  ten_atomic_t file_line_init_failed;
} ten_backtrace_posix_t;

TEN_UTILS_PRIVATE_API int ten_backtrace_dump_using_libgcc(ten_backtrace_t *self,
                                                          size_t skip);

TEN_UTILS_PRIVATE_API int ten_backtrace_dump_using_glibc(ten_backtrace_t *self,
                                                          size_t skip);

/**
 * @brief Read initial debug data from a descriptor, and set the the following
 * fields of @a self.
 * - get_file_line_func
 * - get_file_line_data
 * - get_syminfo_func
 * - get_syminfo_data
 *
 * @param on_get_file_line The value of the on_get_file_line field of @a
 * self.
 * @return 1 on success, 0 on error.
 *
 * @note This function is called after the descriptor has first been opened.
 * It will close the descriptor if it is no longer needed.
 */
TEN_UTILS_PRIVATE_API int ten_backtrace_init_posix(
    ten_backtrace_t *self, const char *filename, int descriptor,
    ten_backtrace_on_error_func_t on_error, void *data,
    ten_backtrace_on_get_file_line_func_t *on_get_file_line);

/**
 * @brief Add file/line information for a DWARF module.
 */
TEN_UTILS_PRIVATE_API int backtrace_dwarf_add(
    ten_backtrace_t *self, uintptr_t base_address,
    const dwarf_sections *dwarf_sections, int is_bigendian,
    dwarf_data *fileline_altlink, ten_backtrace_on_error_func_t on_error,
    void *data, ten_backtrace_on_get_file_line_func_t *on_get_file_line,
    dwarf_data **fileline_entry);

/**
 * @brief A data structure used to adapt symbol information callbacks to
 * file/line callbacks.
 *
 * This structure is used as an adapter when we have symbol information but no
 * debug information. It allows us to convert symbol lookup results into
 * file/line format by storing both the original file/line callback and its
 * associated data.
 *
 * It's primarily used in functions like `elf_nodebug()` to bridge between the
 * symbol lookup interface and the file/line interface that callers expect.
 *
 * @param on_dump_file_line The callback function to report file/line
 * information.
 * @param on_error The callback function to report errors.
 * @param data User data to pass to the callbacks.
 * @param ret Return value indicating whether symbol information was found (1)
 * or not (0).
 */
typedef struct backtrace_call_full {
  ten_backtrace_on_dump_file_line_func_t on_dump_file_line;
  ten_backtrace_on_error_func_t on_error;
  void *data;
  int ret;
} backtrace_call_full;

TEN_UTILS_PRIVATE_API void backtrace_dump_syminfo_to_file_line(
    ten_backtrace_t *self, uintptr_t pc, const char *symname,
    TEN_UNUSED uintptr_t sym_val, TEN_UNUSED uintptr_t sym_size, void *data);

TEN_UTILS_PRIVATE_API void backtrace_dump_syminfo_to_file_line_error(
    ten_backtrace_t *self, const char *msg, int errnum, void *data);
