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

#include <stddef.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/view.h"

/**
 * @brief Information we keep for an ELF symbol.
 *
 * This structure represents a symbol from an ELF file's symbol table. It stores
 * the essential information needed for address-to-symbol lookups during stack
 * trace symbolization.
 */
typedef struct elf_symbol {
  // The name of the symbol (function or variable name).
  const char *name;

  // The memory address where the symbol is loaded.
  uintptr_t address;

  // The size of the symbol in bytes. Used to determine if an address falls
  // within this symbol's range (address <= pc < address + size).
  size_t size;
} elf_symbol;

/**
 * @brief Data structure containing symbol information for an ELF module.
 *
 * This structure holds symbol table information extracted from an ELF file.
 * It's used by the `elf_syminfo` function to look up symbol information for a
 * given address during stack trace symbolization. Multiple instances of this
 * structure can be linked together to form a chain of symbol tables from
 * different loaded modules (executable and shared libraries).
 */
typedef struct elf_syminfo_data {
  // Pointer to symbol data for the next loaded module in the chain. Forms a
  // linked list of symbol tables from different modules.
  struct elf_syminfo_data *next;

  // Array of ELF symbols sorted by address for efficient binary search. Each
  // entry contains a symbol name, address, and size.
  elf_symbol *symbols;

  // The number of entries in the symbols array.
  size_t count;
} elf_syminfo_data;

// Information about PowerPC64 ELFv1 .opd section.
typedef struct elf_ppc64_opd_data {
  // Address of the .opd section.
  b_elf_addr addr;
  // Section data.
  const char *data;
  // Size of the .opd section.
  size_t size;
  // Corresponding section view.
  elf_view view;
} elf_ppc64_opd_data;

TEN_UTILS_PRIVATE_API void elf_add_syminfo_data(ten_backtrace_t *self,
                                                elf_syminfo_data *edata);

TEN_UTILS_PRIVATE_API int elf_initialize_syminfo(
    ten_backtrace_t *self, uintptr_t base_address,
    const unsigned char *symtab_data, size_t symtab_size,
    const unsigned char *strtab, size_t strtab_size,
    ten_backtrace_on_error_func_t on_error, void *data, elf_syminfo_data *sdata,
    elf_ppc64_opd_data *opd);

TEN_UTILS_PRIVATE_API void elf_syminfo(
    ten_backtrace_t *self, uintptr_t addr,
    ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
    ten_backtrace_on_error_func_t on_error, void *data);
