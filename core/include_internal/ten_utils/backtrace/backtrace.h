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

#include <stddef.h>
#include <stdint.h>

#define MAX_CAPTURED_CALL_STACK_DEPTH 128

#if defined(__cplusplus)
extern "C" {
#endif

// This is a virtual type, only used as the type for parameters of public
// functions.
typedef struct ten_backtrace_t ten_backtrace_t;

/**
 * @brief The type of the callback to be called when one backtrace frame is
 * dumping.
 *
 * @param ten_backtrace
 * @param pc The program counter.
 * @param filename The name of the file containing @a pc, or NULL if not
 *                 available.
 * @param LINENO The line number in @a filename containing @a pc, or 0 if not
 *               available.
 * @param function The name of the function containing @a pc, or NULL if
 *                 not available.
 *
 * @return 0 to continuing tracing.
 *
 * @note The @a filename and @a function buffers may become invalid after this
 * function returns.
 */
typedef int (*ten_backtrace_on_dump_file_line_func_t)(
    ten_backtrace_t *ten_backtrace, uintptr_t pc, const char *filename,
    int lineno, const char *function, void *data);

/**
 * @brief The type of the callback to be called when one symbol information is
 * dumping.
 *
 * @param ten_backtrace
 * @param pc The program counter.
 * @param sym_name The name of the symbol for the corresponding code.
 * @param sym_val The value of the symbol.
 * @param sym_size The size of the symbol.
 *
 * @note @a symname will be NULL if no error occurred but the symbol could not
 * be found.
 */
typedef void (*ten_backtrace_on_dump_syminfo_func_t)(
    ten_backtrace_t *ten_backtrace, uintptr_t pc, const char *sym_name,
    uintptr_t sym_val, uintptr_t sym_size, void *data);

/**
 * @brief The type of the error callback to be called when backtrace module
 * encounters errors. This function, if not NULL, will be called for certain
 * error cases.
 *
 * @param ten_backtrace
 * @param msg An error message.
 * @param errnum if greater than 0, holds an errno value.
 *
 * @note The @a msg buffer may become invalid after this function returns.
 * @note As a special case, the @a errnum argument will be passed as -1 if no
 * debug info can be found for the executable, or if the debug info exists but
 * has an unsupported version, but the function requires debug info (e.g.,
 * ten_backtrace_dump). The @a msg in this case will be something along the
 * lines of "no debug info".
 * @note Similarly, @a errnum will be passed as -1 if there is no symbol table,
 * but the function requires a symbol table (e.g., backtrace_syminfo). This may
 * be used as a signal that some other approach should be tried.
 */
typedef void (*ten_backtrace_on_error_func_t)(ten_backtrace_t *self,
                                              const char *msg, int errnum,
                                              void *data);

/**
 * @brief Given @a pc, a program counter in the current program, call the
 * @a on_dump_file_line function with filename, line number, and function name
 * information. This will normally call the callback function exactly once.
 * However, if the @a pc happens to describe an inlined call, and the debugging
 * information contains the necessary information, then this may call the
 * callback function multiple times. This will make at least one call to either
 * @a on_dump_file_line or @a on_error.
 *
 * @return The first non-zero value returned by @a on_dump_file_line or @a
 * on_error, or 0.
 */
TEN_UTILS_PRIVATE_API int ten_backtrace_get_file_line_info(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
    ten_backtrace_on_error_func_t on_error, void *data);

/**
 * @brief Given @a pc, an address or program counter in the current program,
 * call the callback information with the symbol name and value describing the
 * function or variable in which @a pc may be found.
 * This will call either @a on_dump_syminfo or @a on_error exactly once.
 *
 * @return 1 on success, 0 on failure.
 *
 * @note This function requires the symbol table but does not require the debug
 * info. Note that if the symbol table is present but @a pc could not be found
 * in the table, @a on_dump_syminfo will be called with a NULL @a sym_name
 * argument. Returns 1 on success, 0 on error.
 */
TEN_UTILS_PRIVATE_API int ten_backtrace_get_syminfo(
    ten_backtrace_t *self, uintptr_t pc,
    ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
    ten_backtrace_on_error_func_t on_error, void *data);

TEN_UTILS_API void ten_backtrace_create_global(void);

TEN_UTILS_PRIVATE_API ten_backtrace_t *ten_backtrace_create(void);

TEN_UTILS_API void ten_backtrace_destroy_global(void);

TEN_UTILS_PRIVATE_API void ten_backtrace_destroy(ten_backtrace_t *self);

/**
 * @brief Get a full stack backtrace.
 *
 * @param skip The number of frames to skip. Passing 0 will start the trace with
 * the function calling ten_backtrace_dump.
 *
 * @note If any call to 'dump' callback returns a non-zero value, the stack
 * backtrace stops, and backtrace returns that value; this may be used to
 * limit the number of stack frames desired.
 * @note If all calls to 'dump' callback return 0, backtrace returns 0. The
 * ten_backtrace_dump function will make at least one call to either
 * 'dump' callback or 'error' callback.
 * @note This function requires debug info for the executable.
 */
TEN_UTILS_PRIVATE_API void ten_backtrace_dump(ten_backtrace_t *self,
                                              size_t skip);

#if defined(__cplusplus)
} /* End extern "C".  */
#endif
