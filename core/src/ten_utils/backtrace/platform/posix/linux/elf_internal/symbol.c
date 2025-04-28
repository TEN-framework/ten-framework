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

#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/symbol.h"

#include <assert.h>
#include <stdlib.h>

#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf.h"
#include "include_internal/ten_utils/backtrace/sort.h"
#include "ten_utils/lib/atomic_ptr.h"

/**
 * @brief Compare two elf_symbol structures by their address for sorting.
 *
 * This function is used as a comparison callback for qsort to sort an array
 * of `elf_symbol` structures by their address in ascending order. This sorting
 * enables efficient binary search when looking up symbols by address.
 *
 * @param v1 Pointer to the first `elf_symbol` to compare.
 * @param v2 Pointer to the second `elf_symbol` to compare.
 * @return -1 if e1's address is less than e2's,
 *          1 if e1's address is greater than e2's,
 *          0 if the addresses are equal.
 *
 * @note When addresses are equal, we don't consider symbol size or name as
 *       secondary sort keys. If multiple symbols have the same address,
 *       the order between them is not guaranteed to be stable.
 */
static int elf_symbol_compare(const void *v1, const void *v2) {
  const elf_symbol *e1 = (const elf_symbol *)v1;
  const elf_symbol *e2 = (const elf_symbol *)v2;

  if (e1->address < e2->address) {
    return -1;
  } else if (e1->address > e2->address) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief Initialize the symbol table information for backtrace symbol lookup.
 *
 * This function processes the ELF symbol table and string table to build a
 * sorted array of function and object symbols for efficient address lookup
 * during backtracing. It handles special cases like PowerPC64 ELFv1 symbols in
 * the .opd section.
 *
 * @param self The backtrace context.
 * @param base_address The base address where the binary is loaded.
 * @param symtab_data Pointer to the symbol table data.
 * @param symtab_size Size of the symbol table in bytes.
 * @param strtab Pointer to the string table.
 * @param strtab_size Size of the string table in bytes.
 * @param on_error Callback function for reporting errors.
 * @param data User data to pass to the error callback.
 * @param sdata Pointer to the symbol info data structure to initialize.
 * @param opd Optional pointer to PowerPC64 OPD data (can be NULL for non-PPC
 * platforms).
 * @return 1 on success, 0 on failure.
 */
int elf_initialize_syminfo(ten_backtrace_t *self, uintptr_t base_address,
                           const unsigned char *symtab_data, size_t symtab_size,
                           const unsigned char *strtab, size_t strtab_size,
                           ten_backtrace_on_error_func_t on_error, void *data,
                           elf_syminfo_data *sdata, elf_ppc64_opd_data *opd) {
  size_t sym_count = 0;
  const b_elf_sym *sym = NULL;
  size_t elf_symbol_count = 0;
  size_t elf_symbol_size = 0;
  elf_symbol *elf_symbols = NULL;
  size_t i = 0;
  unsigned int j = 0;

  // Input validation
  if (!symtab_data || !strtab || !sdata) {
    if (on_error) {
      on_error(self, "invalid input parameters", 0, data);
    }
    assert(0 && "Invalid input parameters.");
    return 0;
  }

  sym_count = symtab_size / sizeof(b_elf_sym);

  // We only care about function and object symbols. Count them.
  sym = (const b_elf_sym *)symtab_data;
  elf_symbol_count = 0;
  for (i = 0; i < sym_count; ++i, ++sym) {
    int info = sym->st_info & 0xf;
    if ((info == STT_FUNC || info == STT_OBJECT) &&
        sym->st_shndx != SHN_UNDEF) {
      ++elf_symbol_count;
    }
  }

  // Some shared libraries might not have any exported symbols, e.g., RTC
  // plugins. This is not an error condition, just return success.
  if (elf_symbol_count == 0) {
    sdata->next = NULL;
    sdata->symbols = NULL;
    sdata->count = 0;
    return 1;
  }

  elf_symbol_size = elf_symbol_count * sizeof(elf_symbol);
  elf_symbols = (elf_symbol *)malloc(elf_symbol_size);
  if (elf_symbols == NULL) {
    if (on_error) {
      on_error(self, "failed to allocate memory for symbols", 0, data);
    }
    assert(0 && "Failed to allocate memory for symbols.");
    return 0;
  }

  // Extract relevant symbols from the symbol table.
  sym = (const b_elf_sym *)symtab_data;
  j = 0;
  for (i = 0; i < sym_count; ++i, ++sym) {
    int info = sym->st_info & 0xf;
    if (info != STT_FUNC && info != STT_OBJECT) {
      continue;
    }

    if (sym->st_shndx == SHN_UNDEF) {
      continue;
    }

    if (sym->st_name >= strtab_size) {
      on_error(self, "symbol string index out of range", 0, data);
      free(elf_symbols);
      return 0;
    }
    elf_symbols[j].name = (const char *)strtab + sym->st_name;

    // Special case PowerPC64 ELFv1 symbols in .opd section:
    // If the symbol is a function descriptor, read the actual code address from
    // the descriptor. The .opd section contains function descriptors with the
    // actual function address as the first field.
    if (opd && sym->st_value >= opd->addr &&
        sym->st_value < opd->addr + opd->size) {
      elf_symbols[j].address =
          *(const b_elf_addr *)(opd->data + (sym->st_value - opd->addr));
    } else {
      elf_symbols[j].address = sym->st_value;
    }

    // Add the base address to get the absolute address.
    elf_symbols[j].address += base_address;
    elf_symbols[j].size = sym->st_size;
    ++j;
  }

  // Sort symbols by address for efficient binary search.
  backtrace_sort(elf_symbols, elf_symbol_count, sizeof(elf_symbol),
                 elf_symbol_compare);

  // Initialize the symbol info data structure.
  sdata->next = NULL;
  sdata->symbols = elf_symbols;
  sdata->count = elf_symbol_count;

  return 1;
}

/**
 * @brief Adds symbol information data to the backtrace state's linked list.
 *
 * This function atomically appends the provided symbol information data (EDATA)
 * to the end of a linked list maintained in the backtrace state. The function
 * uses atomic operations to ensure thread safety when multiple threads might
 * be adding symbol data concurrently.
 *
 * The function traverses the linked list to find the last element, then
 * uses compare-and-swap to atomically append the new data. If the append
 * fails due to concurrent modification, it retries the entire operation.
 *
 * @param self The backtrace state.
 * @param edata Pointer to the symbol information data to be added.
 *              This should be allocated memory that will persist.
 *
 * @note The caller is responsible for properly initializing edata,
 *       including setting edata->next to NULL before calling this function.
 */
void elf_add_syminfo_data(ten_backtrace_t *self, elf_syminfo_data *edata) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument: NULL backtrace context.");
  assert(edata && "Invalid argument: NULL symbol data.");
  assert(edata->next == NULL &&
         "Symbol data must have next pointer set to NULL.");

  while (1) {
    elf_syminfo_data **pp =
        (elf_syminfo_data **)&self_posix->on_get_syminfo_data;

    // Find the last element in the linked list.
    while (1) {
      elf_syminfo_data *p = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
      if (p == NULL) {
        break;
      }

      pp = &p->next;
    }

    // Try to append the new data atomically.
    if (__sync_bool_compare_and_swap(pp, NULL, edata)) {
      break;
    }

    // If CAS failed, another thread modified the list, so retry.
  }
}

/**
 * @brief Compare an address against an ELF symbol for binary search.
 *
 * This function is used as a comparison callback for `bsearch()` when looking
 * up symbols by address. It determines if the given address falls within the
 * memory range occupied by a symbol (address >= symbol.address &&
 * address < symbol.address + symbol.size).
 *
 * @param vkey Pointer to the address to look up (cast to void*).
 * @param ventry Pointer to the `elf_symbol` structure to compare against (cast
 * to void*).
 * @return -1 if address is less than symbol's address range,
 *          0 if address falls within the symbol's address range,
 *          1 if address is greater than symbol's address range.
 *
 * @note We allocate one extra entry in the symbols array with a very high
 * address to ensure this function can safely check address ranges even for the
 * last real symbol in the array.
 * @note Special care is taken for symbols with size 0, which will only match
 *       if the address exactly equals the symbol's address.
 * @note This function handles potential integer overflow when calculating
 *       entry->address + entry->size.
 */
static int elf_symbol_search(const void *vkey, const void *ventry) {
  const uintptr_t *key = (const uintptr_t *)vkey;
  const elf_symbol *entry = (const elf_symbol *)ventry;
  uintptr_t addr = *key;

  // If address is before the symbol's start address.
  if (addr < entry->address) {
    return -1;
  }

  // Check if this is a sentinel entry (used to mark the end of the array).
  if (entry->name == NULL ||
      (entry->name[0] == '\0' && entry->address == ~(uintptr_t)0)) {
    return -1;
  }

  // Handle potential integer overflow when calculating end address.
  if (entry->size > 0) {
    // Check if entry->address + entry->size would overflow.
    if (UINTPTR_MAX - entry->address < entry->size) {
      // In case of overflow, treat as if address is within range.
      // Since addr can't be larger than UINTPTR_MAX.
      return 0;
    }

    // If address is at or after the symbol's end address (address + size).
    if (addr >= entry->address + entry->size) {
      return 1;
    }
  } else {
    // For symbols with size 0, we only match exact address.
    if (addr > entry->address) {
      return 1;
    }
  }

  // Address is within the symbol's range.
  return 0;
}

/**
 * @brief Look up symbol information for a given address.
 *
 * This function searches through all available symbol tables to find
 * information about the symbol at the specified address. It traverses a linked
 * list of symbol data structures, each containing an array of symbols sorted by
 * address. When a matching symbol is found, it calls the provided callback with
 * the symbol's name, address, and size.
 *
 * The function uses binary search (bsearch) with the `elf_symbol_search`
 * comparison function to efficiently locate symbols in potentially large
 * symbol tables.
 *
 * @param self The backtrace context.
 * @param addr The address to look up.
 * @param on_dump_syminfo Function to call with the symbol information if found.
 *        Will be called with NULL name if no symbol is found.
 * @param on_error Function to call to report errors (currently unused).
 * @param data User data to pass to the callback functions.
 */
void elf_syminfo(ten_backtrace_t *self, uintptr_t addr,
                 ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
                 TEN_UNUSED ten_backtrace_on_error_func_t on_error,
                 void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  elf_symbol *sym = NULL;

  // Get the head of the linked list of symbol data structures.
  elf_syminfo_data **pp = (elf_syminfo_data **)&self_posix->on_get_syminfo_data;

  // Traverse the linked list of symbol data structures.
  while (1) {
    elf_syminfo_data *edata = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
    if (edata == NULL) {
      break;  // End of the list.
    }

    // Skip empty symbol tables.
    if (edata->symbols == NULL || edata->count == 0) {
      pp = &edata->next;
      continue;
    }

    // Binary search for the symbol containing the address.
    sym = (elf_symbol *)bsearch(&addr, edata->symbols, edata->count,
                                sizeof(elf_symbol), elf_symbol_search);
    if (sym != NULL) {
      break;  // Found a matching symbol.
    }

    // Move to the next symbol data structure.
    pp = &edata->next;
  }

  // Call the callback with the symbol information or NULL if not found.
  if (sym == NULL) {
    on_dump_syminfo(self, addr, NULL, 0, 0, data);
  } else {
    on_dump_syminfo(self, addr, sym->name, sym->address, sym->size, data);
  }
}
