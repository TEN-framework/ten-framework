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
#include <dirent.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"
#include "include_internal/ten_utils/backtrace/platform/posix/file.h"
#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/mmap.h"
#include "include_internal/ten_utils/backtrace/sort.h"
#include "ten_utils/lib/atomic_ptr.h"

// Mach-O file header for a 32-bit executable.
typedef struct macho_header_32 {
  uint32_t magic;       // Magic number (MACH_O_MAGIC_32)
  uint32_t cputype;     // CPU type
  uint32_t cpusubtype;  // CPU subtype
  uint32_t filetype;    // Type of file (object, executable)
  uint32_t ncmds;       // Number of load commands
  uint32_t sizeofcmds;  // Total size of load commands
  uint32_t flags;       // Flags for special features
} macho_header_32;

// Mach-O file header for a 64-bit executable.
typedef struct macho_header_64 {
  uint32_t magic;       // Magic number (MACH_O_MAGIC_64)
  uint32_t cputype;     // CPU type
  uint32_t cpusubtype;  // CPU subtype
  uint32_t filetype;    // Type of file (object, executable)
  uint32_t ncmds;       // Number of load commands
  uint32_t sizeofcmds;  // Total size of load commands
  uint32_t flags;       // Flags for special features
  uint32_t reserved;    // Reserved
} macho_header_64;

// Mach-O file header for a fat executable.
typedef struct macho_header_fat {
  uint32_t magic;      // Magic number (MACH_O_MH_(MAGIC|CIGAM)_FAT(_64)?)
  uint32_t nfat_arch;  // Number of components
} macho_header_fat;

// Values for the header magic field.
#define MACH_O_MH_MAGIC_32 0xfeedface
#define MACH_O_MH_MAGIC_64 0xfeedfacf
#define MACH_O_MH_MAGIC_FAT 0xcafebabe
#define MACH_O_MH_CIGAM_FAT 0xbebafeca
#define MACH_O_MH_MAGIC_FAT_64 0xcafebabf
#define MACH_O_MH_CIGAM_FAT_64 0xbfbafeca

// Value for the header filetype field.
#define MACH_O_MH_EXECUTE 0x02
#define MACH_O_MH_DYLIB 0x06
#define MACH_O_MH_DSYM 0x0a

/**
 * @brief A component of a fat file. A fat file starts with a macho_header_fat
 * followed by nfat_arch instances of this struct.
 */
typedef struct macho_fat_arch {
  uint32_t cputype;     // CPU type
  uint32_t cpusubtype;  // CPU subtype
  uint32_t offset;      // File offset of this entry
  uint32_t size;        // Size of this entry
  uint32_t align;       // Alignment of this entry
} macho_fat_arch;

/**
 * @brief A component of a 64-bit fat file. This is used if the magic field is
 * MAGIC_FAT_64. This is only used when some file size or file offset is too
 * large to represent in the 32-bit format.
 */
typedef struct macho_fat_arch_64 {
  uint32_t cputype;     // CPU type
  uint32_t cpusubtype;  // CPU subtype
  uint64_t offset;      // File offset of this entry
  uint64_t size;        // Size of this entry
  uint32_t align;       // Alignment of this entry
  uint32_t reserved;    // Reserved
} macho_fat_arch_64;

// Values for the fat_arch cputype field (and the header cputype field).
#define MACH_O_CPU_ARCH_ABI64 0x01000000

#define MACH_O_CPU_TYPE_X86 7
#define MACH_O_CPU_TYPE_ARM 12
#define MACH_O_CPU_TYPE_PPC 18

#define MACH_O_CPU_TYPE_X86_64 (MACH_O_CPU_TYPE_X86 | MACH_O_CPU_ARCH_ABI64)
#define MACH_O_CPU_TYPE_ARM64 (MACH_O_CPU_TYPE_ARM | MACH_O_CPU_ARCH_ABI64)
#define MACH_O_CPU_TYPE_PPC64 (MACH_O_CPU_TYPE_PPC | MACH_O_CPU_ARCH_ABI64)

// The header of a load command.
typedef struct macho_load_command {
  uint32_t cmd;      // The type of load command
  uint32_t cmdsize;  // Size in bytes of the entire command
} macho_load_command;

// Values for the load_command cmd field.
#define MACH_O_LC_SEGMENT 0x01
#define MACH_O_LC_SYMTAB 0x02
#define MACH_O_LC_SEGMENT_64 0x19
#define MACH_O_LC_UUID 0x1b

// The length of a section of segment name.
#define MACH_O_NAMELEN (16)

// LC_SEGMENT load command.
typedef struct macho_segment_command {
  uint32_t cmd;                  // The type of load command (LC_SEGMENT)
  uint32_t cmdsize;              // Size in bytes of the entire command
  char segname[MACH_O_NAMELEN];  // Segment name
  uint32_t vmaddr;               // Virtual memory address
  uint32_t vmsize;               // Virtual memory size
  uint32_t fileoff;              // Offset of data to be mapped
  uint32_t filesize;             // Size of data in file
  uint32_t maxprot;              // Maximum permitted virtual protection
  uint32_t initprot;             // Initial virtual memory protection
  uint32_t nsects;               // Number of sections in this segment
  uint32_t flags;                // Flags
} macho_segment_command;

// LC_SEGMENT_64 load command.
typedef struct macho_segment_64_command {
  uint32_t cmd;                  // The type of load command (LC_SEGMENT)
  uint32_t cmdsize;              // Size in bytes of the entire command
  char segname[MACH_O_NAMELEN];  // Segment name
  uint64_t vmaddr;               // Virtual memory address
  uint64_t vmsize;               // Virtual memory size
  uint64_t fileoff;              // Offset of data to be mapped
  uint64_t filesize;             // Size of data in file
  uint32_t maxprot;              // Maximum permitted virtual protection
  uint32_t initprot;             // Initial virtual memory protection
  uint32_t nsects;               // Number of sections in this segment
  uint32_t flags;                // Flags
} macho_segment_64_command;

// LC_SYMTAB load command.
typedef struct macho_symtab_command {
  uint32_t cmd;      // The type of load command (LC_SEGMENT)
  uint32_t cmdsize;  // Size in bytes of the entire command
  uint32_t symoff;   // File offset of symbol table
  uint32_t nsyms;    // Number of symbols
  uint32_t stroff;   // File offset of string table
  uint32_t strsize;  // String table size
} macho_symtab_command;

// The length of a Mach-O uuid.
#define MACH_O_UUID_LEN (16)

// LC_UUID load command.
typedef struct macho_uuid_command {
  uint32_t cmd;                         // Type of load command (LC_UUID)
  uint32_t cmdsize;                     // Size in bytes of command
  unsigned char uuid[MACH_O_UUID_LEN];  // UUID
} macho_uuid_command;

// 32-bit section header within a LC_SEGMENT segment.
typedef struct macho_section {
  char sectname[MACH_O_NAMELEN];  // Section name
  char segment[MACH_O_NAMELEN];   // Segment of this section
  uint32_t addr;                  // Address in memory
  uint32_t size;                  // Section size
  uint32_t offset;                // File offset
  uint32_t align;                 // Log2 of section alignment
  uint32_t reloff;                // File offset of relocations
  uint32_t nreloc;                // Number of relocs for this section
  uint32_t flags;                 // Flags
  uint32_t reserved1;
  uint32_t reserved2;
} macho_section;

// 64-bit section header within a LC_SEGMENT_64 segment.
typedef struct macho_section_64 {
  char sectname[MACH_O_NAMELEN];  // Section name
  char segment[MACH_O_NAMELEN];   // Segment of this section
  uint64_t addr;                  // Address in memory
  uint64_t size;                  // Section size
  uint32_t offset;                // File offset
  uint32_t align;                 // Log2 of section alignment
  uint32_t reloff;                // File offset of section relocations
  uint32_t nreloc;                // Number of relocs for this section
  uint32_t flags;                 // Flags
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
} macho_section_64;

// 32-bit symbol data.
typedef struct macho_nlist {
  uint32_t n_strx;   // Index of name in string table
  uint8_t n_type;    // Type flag
  uint8_t n_sect;    // Section number
  uint16_t n_desc;   // Stabs description field
  uint32_t n_value;  // Value
} macho_nlist;

// 64-bit symbol data.
typedef struct macho_nlist_64 {
  uint32_t n_strx;   // Index of name in string table
  uint8_t n_type;    // Type flag
  uint8_t n_sect;    // Section number
  uint16_t n_desc;   // Stabs description field
  uint64_t n_value;  // Value
} macho_nlist_64;

// Value found in nlist n_type field.
#define MACH_O_N_EXT 0x01   // Extern symbol
#define MACH_O_N_ABS 0x02   // Absolute symbol
#define MACH_O_N_SECT 0x0e  // Defined in section

#define MACH_O_N_TYPE 0x0e  // Mask for type bits
#define MACH_O_N_STAB 0xe0  // Stabs debugging symbol

/**
 * @brief Information we keep for a Mach-O symbol.
 *
 * This structure represents a symbol from a Mach-O file's symbol table. It
 * stores the essential information needed for address-to-symbol lookups during
 * stack trace symbolization.
 */
typedef struct macho_symbol {
  const char *name;   // Symbol name (points to string table, not owned).
  uintptr_t address;  // Symbol address (adjusted with base address).
} macho_symbol;

/**
 * @brief Data structure containing symbol information for a Mach-O module.
 *
 * This structure holds symbol table information extracted from a Mach-O file.
 * It's used by the `macho_syminfo` function to look up symbol information for a
 * given address during stack trace symbolization. Multiple instances of this
 * structure can be linked together to form a chain of symbol tables from
 * different loaded modules (executable and shared libraries).
 */
typedef struct macho_syminfo_data {
  struct macho_syminfo_data *next;  // Next module in the linked list
  struct macho_symbol *symbols;     // Array of symbols sorted by address
  size_t count;                     // Number of symbols in the array
} macho_syminfo_data;

// Names of sections, indexed by enum dwarf_section in internal.h.
static const char *const dwarf_section_names[DEBUG_MAX] = {
    "__debug_info",     "__debug_line",
    "__debug_abbrev",   "__debug_ranges",
    "__debug_str",      "",  // DEBUG_ADDR
    "__debug_str_offs", "",  // DEBUG_LINE_STR
    "__debug_rnglists"};

static int macho_add(ten_backtrace_t *self, const char *filename,
                     int descriptor, off_t offset,
                     const unsigned char *match_uuid, uintptr_t base_address,
                     int skip_symtab, ten_backtrace_on_error_func_t on_error,
                     void *data,
                     ten_backtrace_on_get_file_line_func_t *fileline_fn,
                     int *found_sym);

// A dummy callback function used when we can't find any debug info.
static int macho_nodebug(ten_backtrace_t *self, TEN_UNUSED uintptr_t pc,
                         TEN_UNUSED ten_backtrace_on_dump_file_line_func_t
                             on_dump_file_line,
                         ten_backtrace_on_error_func_t on_error, void *data) {
  on_error(self, "no debug info in Mach-O executable", -1, data);
  return 0;
}

// A dummy callback function used when we can't find a symbol table.
static void macho_nosyms(ten_backtrace_t *self, TEN_UNUSED uintptr_t addr,
                         TEN_UNUSED ten_backtrace_on_dump_syminfo_func_t
                             on_dump_syminfo,
                         ten_backtrace_on_error_func_t on_error, void *data) {
  on_error(self, "no symbol table in Mach-O executable", -1, data);
}

/**
 * @brief Add a single DWARF section to DWARF_SECTIONS, if we need the section.
 *
 * @return 1 on success, 0 on failure.
 */
static int macho_add_dwarf_section(
    TEN_UNUSED ten_backtrace_t *self, int descriptor, const char *sectname,
    uint32_t offset, uint64_t size,
    TEN_UNUSED ten_backtrace_on_error_func_t on_error, TEN_UNUSED void *data,
    dwarf_sections *dwarf_sections) {
  for (size_t i = 0; i < DEBUG_MAX; ++i) {
    if (dwarf_section_names[i][0] != '\0' &&
        strncmp(sectname, dwarf_section_names[i], MACH_O_NAMELEN) == 0) {
      ten_mmap_t section_view;

      // TODO(Wei): Perhaps it would be better to try to use a single view to
      // read all the DWARF data, as we try to do for ELF.

      if (!ten_mmap_init(&section_view, descriptor, offset, size)) {
        return 0;
      }

      dwarf_sections->data[i] = (const unsigned char *)section_view.data;
      dwarf_sections->size[i] = size;
      break;
    }
  }
  return 1;
}

// Collect DWARF sections from a DWARF segment.  Returns 1 on success,
// 0 on failure.
static int macho_add_dwarf_segment(ten_backtrace_t *self, int descriptor,
                                   off_t offset, unsigned int cmd,
                                   const char *psecs, size_t sizesecs,
                                   unsigned int nsects,
                                   ten_backtrace_on_error_func_t on_error,
                                   void *data, dwarf_sections *dwarf_sections) {
  size_t sec_header_size = 0;
  size_t secoffset = 0;
  unsigned int i = 0;

  switch (cmd) {
  case MACH_O_LC_SEGMENT:
    sec_header_size = sizeof(struct macho_section);
    break;
  case MACH_O_LC_SEGMENT_64:
    sec_header_size = sizeof(struct macho_section_64);
    break;
  default:
    abort();
  }

  secoffset = 0;
  for (i = 0; i < nsects; ++i) {
    if (secoffset + sec_header_size > sizesecs) {
      on_error(self, "section overflow withing segment", 0, data);
      return 0;
    }

    switch (cmd) {
    case MACH_O_LC_SEGMENT: {
      struct macho_section section;

      memcpy(&section, psecs + secoffset, sizeof section);
      macho_add_dwarf_section(self, descriptor, section.sectname,
                              offset + section.offset, section.size, on_error,
                              data, dwarf_sections);
    } break;

    case MACH_O_LC_SEGMENT_64: {
      struct macho_section_64 section;

      memcpy(&section, psecs + secoffset, sizeof section);
      macho_add_dwarf_section(self, descriptor, section.sectname,
                              offset + section.offset, section.size, on_error,
                              data, dwarf_sections);
    } break;

    default:
      abort();
    }

    secoffset += sec_header_size;
  }

  return 1;
}

// Compare struct macho_symbol for qsort.

static int macho_symbol_compare(const void *v1, const void *v2) {
  const struct macho_symbol *m1 = (const struct macho_symbol *)v1;
  const struct macho_symbol *m2 = (const struct macho_symbol *)v2;

  if (m1->address < m2->address) {
    return -1;
  } else if (m1->address > m2->address) {
    return 1;
  } else {
    return 0;
  }
}

// Compare an address against a macho_symbol for bsearch.  We allocate
// one extra entry in the array so that this can safely look at the
// next entry.
static int macho_symbol_search(const void *vkey, const void *ventry) {
  const uintptr_t *key = (const uintptr_t *)vkey;
  const struct macho_symbol *entry = (const struct macho_symbol *)ventry;
  uintptr_t addr = *key;

  if (addr < entry->address) {
    return -1;
  } else if (entry->name[0] == '\0' && entry->address == ~(uintptr_t)0) {
    return -1;
  } else if ((entry + 1)->name[0] == '\0' &&
             (entry + 1)->address == ~(uintptr_t)0) {
    return -1;
  } else if (addr >= (entry + 1)->address) {
    return 1;
  } else {
    return 0;
  }
}

// Return whether the symbol type field indicates a symbol table entry
// that we care about: a function or data symbol.
static int macho_defined_symbol(uint8_t type) {
  if ((type & MACH_O_N_STAB) != 0) {
    return 0;
  }
  if ((type & MACH_O_N_EXT) != 0) {
    return 0;
  }
  switch (type & MACH_O_N_TYPE) {
  case MACH_O_N_ABS:
  case MACH_O_N_SECT:
    return 1;
  default:
    return 0;
  }
}

// Add symbol table information for a Mach-O file.
static int macho_add_symtab(ten_backtrace_t *self, int descriptor,
                            uintptr_t base_address, int is_64, off_t symoff,
                            unsigned int nsyms, off_t stroff,
                            unsigned int strsize,
                            ten_backtrace_on_error_func_t on_error,
                            void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  size_t symsize = 0;
  ten_mmap_t sym_view;
  int sym_view_valid = 0;
  ten_mmap_t str_view;
  int str_view_valid = 0;
  size_t ndefs = 0;
  size_t symtaboff = 0;
  unsigned int i = 0;
  size_t macho_symbol_size = 0;
  struct macho_symbol *macho_symbols = NULL;
  unsigned int j = 0;
  struct macho_syminfo_data *sdata = NULL;

  sym_view_valid = 0;
  str_view_valid = 0;
  macho_symbol_size = 0;
  macho_symbols = NULL;

  if (is_64) {
    symsize = sizeof(struct macho_nlist_64);
  } else {
    symsize = sizeof(struct macho_nlist);
  }

  if (!ten_mmap_init(&sym_view, descriptor, symoff, nsyms * symsize)) {
    goto fail;
  }

  sym_view_valid = 1;

  if (!ten_mmap_init(&str_view, descriptor, stroff, strsize)) {
    return 0;
  }

  str_view_valid = 1;

  ndefs = 0;
  symtaboff = 0;
  for (i = 0; i < nsyms; ++i, symtaboff += symsize) {
    if (is_64) {
      struct macho_nlist_64 nlist;

      memcpy(&nlist, (const char *)sym_view.data + symtaboff, sizeof nlist);
      if (macho_defined_symbol(nlist.n_type)) {
        ++ndefs;
      }
    } else {
      struct macho_nlist nlist;

      memcpy(&nlist, (const char *)sym_view.data + symtaboff, sizeof nlist);
      if (macho_defined_symbol(nlist.n_type)) {
        ++ndefs;
      }
    }
  }

  // Add 1 to ndefs to make room for a sentinel.
  macho_symbol_size = (ndefs + 1) * sizeof(struct macho_symbol);
  macho_symbols = (struct macho_symbol *)malloc(macho_symbol_size);
  if (macho_symbols == NULL) {
    goto fail;
  }

  j = 0;
  symtaboff = 0;
  for (i = 0; i < nsyms; ++i, symtaboff += symsize) {
    uint32_t strx = 0;
    uint64_t value = 0;
    const char *name = NULL;

    strx = 0;
    value = 0;
    if (is_64) {
      struct macho_nlist_64 nlist;

      memcpy(&nlist, (const char *)sym_view.data + symtaboff, sizeof nlist);
      if (!macho_defined_symbol(nlist.n_type)) {
        continue;
      }

      strx = nlist.n_strx;
      value = nlist.n_value;
    } else {
      struct macho_nlist nlist;

      memcpy(&nlist, (const char *)sym_view.data + symtaboff, sizeof nlist);
      if (!macho_defined_symbol(nlist.n_type)) {
        continue;
      }

      strx = nlist.n_strx;
      value = nlist.n_value;
    }

    if (strx >= strsize) {
      on_error(self, "symbol string index out of range", 0, data);
      goto fail;
    }

    name = (const char *)str_view.data + strx;
    if (name[0] == '_') {
      ++name;
    }
    macho_symbols[j].name = name;
    macho_symbols[j].address = value + base_address;
    ++j;
  }

  sdata = (struct macho_syminfo_data *)malloc(sizeof *sdata);
  assert(sdata && "Failed to allocate memory.");
  if (sdata == NULL) {
    goto fail;
  }

  // We need to keep the string table since it holds the names, but we
  // can release the symbol table.

  ten_mmap_deinit(&sym_view);
  sym_view_valid = 0;
  str_view_valid = 0;

  // Add a trailing sentinel symbol.
  macho_symbols[j].name = "";
  macho_symbols[j].address = ~(uintptr_t)0;

  backtrace_sort(macho_symbols, ndefs + 1, sizeof(struct macho_symbol),
                 macho_symbol_compare);

  sdata->next = NULL;
  sdata->symbols = macho_symbols;
  sdata->count = ndefs;

  while (1) {
    macho_syminfo_data **pp =
        (macho_syminfo_data **)&self_posix->on_get_syminfo_data;

    while (1) {
      macho_syminfo_data *p = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
      if (p == NULL) {
        break;
      }

      pp = &p->next;
    }

    if (__sync_bool_compare_and_swap(pp, NULL, sdata)) {
      break;
    }
  }

  return 1;

fail:
  if (macho_symbols != NULL) {
    free(macho_symbols);
  }

  if (sym_view_valid) {
    ten_mmap_deinit(&sym_view);
  }

  if (str_view_valid) {
    ten_mmap_deinit(&str_view);
  }

  return 0;
}

// Return the symbol name and value for an ADDR.
static void macho_syminfo(ten_backtrace_t *self, uintptr_t addr,
                          ten_backtrace_on_dump_syminfo_func_t on_dump_syminfo,
                          TEN_UNUSED ten_backtrace_on_error_func_t on_error,
                          void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  macho_syminfo_data *sdata = NULL;
  macho_symbol *sym = NULL;

  sym = NULL;

  macho_syminfo_data **pp =
      (macho_syminfo_data **)&self_posix->on_get_syminfo_data;

  while (1) {
    sdata = ten_atomic_ptr_load((ten_atomic_ptr_t *)pp);
    if (sdata == NULL) {
      break;
    }

    sym = ((macho_symbol *)bsearch(&addr, sdata->symbols, sdata->count,
                                   sizeof(struct macho_symbol),
                                   macho_symbol_search));
    if (sym != NULL) {
      break;
    }

    pp = &sdata->next;
  }

  if (sym == NULL) {
    on_dump_syminfo(self, addr, NULL, 0, 0, data);
  } else {
    on_dump_syminfo(self, addr, sym->name, sym->address, 0, data);
  }
}

// Look through a fat file to find the relevant executable.  Returns 1
// on success, 0 on failure (in both cases descriptor is closed).
static int macho_add_fat(ten_backtrace_t *self, const char *filename,
                         int descriptor, int swapped, off_t offset,
                         const unsigned char *match_uuid,
                         uintptr_t base_address, int skip_symtab,
                         uint32_t nfat_arch, int is_64,
                         ten_backtrace_on_error_func_t on_error, void *data,
                         ten_backtrace_on_get_file_line_func_t *fileline_fn,
                         int *found_sym) {
  int arch_view_valid = 0;
  unsigned int cputype = 0;
  size_t arch_size = 0;
  ten_mmap_t arch_view;
  unsigned int i = 0;

  arch_view_valid = 0;

#if defined(__x86_64__)
  cputype = MACH_O_CPU_TYPE_X86_64;
#elif defined(__i386__)
  cputype = MACH_O_CPU_TYPE_X86;
#elif defined(__aarch64__)
  cputype = MACH_O_CPU_TYPE_ARM64;
#elif defined(__arm__)
  cputype = MACH_O_CPU_TYPE_ARM;
#elif defined(__ppc__)
  cputype = MACH_O_CPU_TYPE_PPC;
#elif defined(__ppc64__)
  cputype = MACH_O_CPU_TYPE_PPC64;
#else
  on_error(data, "unknown Mach-O architecture", 0);
  goto fail;
#endif

  if (is_64) {
    arch_size = sizeof(struct macho_fat_arch_64);
  } else {
    arch_size = sizeof(struct macho_fat_arch);
  }

  if (!ten_mmap_init(&arch_view, descriptor, offset, nfat_arch * arch_size)) {
    goto fail;
  }

  for (i = 0; i < nfat_arch; ++i) {
    uint32_t fcputype = 0;
    uint64_t foffset = 0;

    if (is_64) {
      struct macho_fat_arch_64 fat_arch_64;

      memcpy(&fat_arch_64, (const char *)arch_view.data + (i * arch_size),
             arch_size);
      fcputype = fat_arch_64.cputype;
      foffset = fat_arch_64.offset;
      if (swapped) {
        fcputype = __builtin_bswap32(fcputype);
        foffset = __builtin_bswap64(foffset);
      }
    } else {
      struct macho_fat_arch fat_arch_32;

      memcpy(&fat_arch_32, (const char *)arch_view.data + (i * arch_size),
             arch_size);
      fcputype = fat_arch_32.cputype;
      foffset = (uint64_t)fat_arch_32.offset;
      if (swapped) {
        fcputype = __builtin_bswap32(fcputype);
        foffset = (uint64_t)__builtin_bswap32((uint32_t)foffset);
      }
    }

    if (fcputype == cputype) {
      // FIXME: What about cpusubtype?
      ten_mmap_deinit(&arch_view);
      return macho_add(self, filename, descriptor, foffset, match_uuid,
                       base_address, skip_symtab, on_error, data, fileline_fn,
                       found_sym);
    }
  }

  on_error(self, "could not find executable in fat file", 0, data);

fail:
  if (arch_view_valid) {
    ten_mmap_deinit(&arch_view);
  }
  if (descriptor != -1) {
    ten_backtrace_close_file(descriptor);
  }
  return 0;
}

// Look for the dsym file for FILENAME.  This is called if FILENAME
// does not have debug info or a symbol table.  Returns 1 on success,
// 0 on failure.
static int macho_add_dsym(ten_backtrace_t *self, const char *filename,
                          uintptr_t base_address, const unsigned char *uuid,
                          ten_backtrace_on_error_func_t on_error, void *data,
                          ten_backtrace_on_get_file_line_func_t *fileline_fn) {
  const char *p = NULL;
  const char *dirname = NULL;
  char *diralc = NULL;
  size_t dirnamelen = 0;
  const char *basename = NULL;
  size_t basenamelen = 0;
  const char *dsymsuffixdir = NULL;
  size_t dsymsuffixdirlen = 0;
  size_t dsymlen = 0;
  char *dsym = NULL;
  char *ps = NULL;
  bool does_not_exist = false;
  int dummy_found_sym = 0;

  diralc = NULL;
  dirnamelen = 0;
  dsym = NULL;
  dsymlen = 0;

  p = strrchr(filename, '/');
  if (p == NULL) {
    dirname = ".";
    dirnamelen = 1;
    basename = filename;
    basenamelen = strlen(basename);
    diralc = NULL;
  } else {
    dirnamelen = p - filename;
    diralc = malloc(dirnamelen + 1);
    if (diralc == NULL) {
      goto fail;
    }

    memcpy(diralc, filename, dirnamelen);
    diralc[dirnamelen] = '\0';
    dirname = diralc;
    basename = p + 1;
    basenamelen = strlen(basename);
  }

  dsymsuffixdir = ".dSYM/Contents/Resources/DWARF/";
  dsymsuffixdirlen = strlen(dsymsuffixdir);

  dsymlen = (dirnamelen + 1 + basenamelen + dsymsuffixdirlen + basenamelen + 1);
  dsym = malloc(dsymlen);
  assert(dsym && "Failed to allocate memory.");
  if (dsym == NULL) {
    goto fail;
  }

  ps = dsym;
  memcpy(ps, dirname, dirnamelen);
  ps += dirnamelen;
  *ps++ = '/';
  memcpy(ps, basename, basenamelen);
  ps += basenamelen;
  memcpy(ps, dsymsuffixdir, dsymsuffixdirlen);
  ps += dsymsuffixdirlen;
  memcpy(ps, basename, basenamelen);
  ps += basenamelen;
  *ps = '\0';

  if (diralc != NULL) {
    free(diralc);
    diralc = NULL;
  }

  int fd = ten_backtrace_open_file(dsym, &does_not_exist);
  if (fd < 0) {
    // The file does not exist, so we can't read the debug info.
    // Just return success.
    free(dsym);
    return 1;
  }

  if (!macho_add(self, dsym, fd, 0, uuid, base_address, 1, on_error, data,
                 fileline_fn, &dummy_found_sym)) {
    goto fail;
  }

  free(dsym);

  return 1;

fail:
  if (dsym != NULL) {
    free(dsym);
  }
  if (diralc != NULL) {
    free(diralc);
  }
  return 0;
}

// Add the backtrace data for a Macho-O file.  Returns 1 on success, 0
// on failure (in both cases descriptor is closed).
//
// FILENAME: the name of the executable.
// DESCRIPTOR: an open descriptor for the executable, closed here.
// OFFSET: the offset within the file of this executable, for fat files.
// MATCH_UUID: if not NULL, UUID that must match.
// BASE_ADDRESS: the load address of the executable.
// SKIP_SYMTAB: if non-zero, ignore the symbol table; used for dSYM files.
// FILELINE_FN: set to the fileline function, by backtrace_dwarf_add.
// FOUND_SYM: set to non-zero if we found the symbol table.

static int macho_add(ten_backtrace_t *self, const char *filename,
                     int descriptor, off_t offset,
                     const unsigned char *match_uuid, uintptr_t base_address,
                     int skip_symtab, ten_backtrace_on_error_func_t on_error,
                     void *data,
                     ten_backtrace_on_get_file_line_func_t *fileline_fn,
                     int *found_sym) {
  ten_mmap_t header_view;
  struct macho_header_32 header;
  off_t hdroffset = 0;
  int is_64 = 0;
  ten_mmap_t cmds_view;
  int cmds_view_valid = 0;
  dwarf_sections dwarf_sections;
  int have_dwarf = 0;
  unsigned char uuid[MACH_O_UUID_LEN];
  int have_uuid = 0;
  size_t cmdoffset = 0;
  unsigned int i = 0;

  *found_sym = 0;

  cmds_view_valid = 0;

  // The 32-bit and 64-bit file headers start out the same, so we can
  // just always read the 32-bit version.  A fat header is shorter but
  // it will always be followed by data, so it's OK to read extra.

  if (!ten_mmap_init(&header_view, descriptor, offset,
                     sizeof(struct macho_header_32))) {
    goto fail;
  }

  memcpy(&header, header_view.data, sizeof header);

  ten_mmap_deinit(&header_view);

  switch (header.magic) {
  case MACH_O_MH_MAGIC_32:
    is_64 = 0;
    hdroffset = offset + sizeof(struct macho_header_32);
    break;
  case MACH_O_MH_MAGIC_64:
    is_64 = 1;
    hdroffset = offset + sizeof(struct macho_header_64);
    break;
  case MACH_O_MH_MAGIC_FAT:
  case MACH_O_MH_MAGIC_FAT_64: {
    struct macho_header_fat fat_header;

    hdroffset = offset + sizeof(struct macho_header_fat);
    memcpy(&fat_header, &header, sizeof fat_header);
    return macho_add_fat(self, filename, descriptor, 0, hdroffset, match_uuid,
                         base_address, skip_symtab, fat_header.nfat_arch,
                         header.magic == MACH_O_MH_MAGIC_FAT_64, on_error, data,
                         fileline_fn, found_sym);
  }
  case MACH_O_MH_CIGAM_FAT:
  case MACH_O_MH_CIGAM_FAT_64: {
    struct macho_header_fat fat_header;
    uint32_t nfat_arch = 0;

    hdroffset = offset + sizeof(struct macho_header_fat);
    memcpy(&fat_header, &header, sizeof fat_header);
    nfat_arch = __builtin_bswap32(fat_header.nfat_arch);
    return macho_add_fat(self, filename, descriptor, 1, hdroffset, match_uuid,
                         base_address, skip_symtab, nfat_arch,
                         header.magic == MACH_O_MH_CIGAM_FAT_64, on_error, data,
                         fileline_fn, found_sym);
  }
  default:
    on_error(self, "executable file is not in Mach-O format", 0, data);
    goto fail;
  }

  switch (header.filetype) {
  case MACH_O_MH_EXECUTE:
  case MACH_O_MH_DYLIB:
  case MACH_O_MH_DSYM:
    break;
  default:
    on_error(self, "executable file is not an executable", 0, data);
    goto fail;
  }

  if (!ten_mmap_init(&cmds_view, descriptor, hdroffset, header.sizeofcmds)) {
    goto fail;
  }

  cmds_view_valid = 1;

  memset(&dwarf_sections, 0, sizeof dwarf_sections);
  have_dwarf = 0;
  memset(&uuid, 0, sizeof uuid);
  have_uuid = 0;

  cmdoffset = 0;
  for (i = 0; i < header.ncmds; ++i) {
    const char *pcmd = NULL;
    struct macho_load_command load_command;

    if (cmdoffset + sizeof load_command > header.sizeofcmds) {
      break;
    }

    pcmd = (const char *)cmds_view.data + cmdoffset;
    memcpy(&load_command, pcmd, sizeof load_command);

    switch (load_command.cmd) {
    case MACH_O_LC_SEGMENT: {
      struct macho_segment_command segcmd;

      memcpy(&segcmd, pcmd, sizeof segcmd);
      if (memcmp(segcmd.segname, "__DWARF\0\0\0\0\0\0\0\0\0", MACH_O_NAMELEN) ==
          0) {
        if (!macho_add_dwarf_segment(
                self, descriptor, offset, load_command.cmd,
                pcmd + sizeof segcmd, (load_command.cmdsize - sizeof segcmd),
                segcmd.nsects, on_error, data, &dwarf_sections)) {
          goto fail;
        }
        have_dwarf = 1;
      }
    } break;

    case MACH_O_LC_SEGMENT_64: {
      struct macho_segment_64_command segcmd;

      memcpy(&segcmd, pcmd, sizeof segcmd);
      if (memcmp(segcmd.segname, "__DWARF\0\0\0\0\0\0\0\0\0", MACH_O_NAMELEN) ==
          0) {
        if (!macho_add_dwarf_segment(
                self, descriptor, offset, load_command.cmd,
                pcmd + sizeof segcmd, (load_command.cmdsize - sizeof segcmd),
                segcmd.nsects, on_error, data, &dwarf_sections)) {
          goto fail;
        }
        have_dwarf = 1;
      }
    } break;

    case MACH_O_LC_SYMTAB:
      if (!skip_symtab) {
        struct macho_symtab_command symcmd;

        memcpy(&symcmd, pcmd, sizeof symcmd);
        if (!macho_add_symtab(self, descriptor, base_address, is_64,
                              offset + symcmd.symoff, symcmd.nsyms,
                              offset + symcmd.stroff, symcmd.strsize, on_error,
                              data)) {
          goto fail;
        }

        *found_sym = 1;
      }
      break;

    case MACH_O_LC_UUID: {
      struct macho_uuid_command uuidcmd;

      memcpy(&uuidcmd, pcmd, sizeof uuidcmd);
      memcpy(&uuid[0], &uuidcmd.uuid[0], MACH_O_UUID_LEN);
      have_uuid = 1;
    } break;

    default:
      break;
    }

    cmdoffset += load_command.cmdsize;
  }

  if (!ten_backtrace_close_file(descriptor)) {
    goto fail;
  }
  descriptor = -1;

  ten_mmap_deinit(&cmds_view);
  cmds_view_valid = 0;

  if (match_uuid != NULL) {
    // If we don't have a UUID, or it doesn't match, just ignore
    // this file.
    if (!have_uuid || memcmp(match_uuid, &uuid[0], MACH_O_UUID_LEN) != 0) {
      return 1;
    }
  }

  if (have_dwarf) {
    int is_big_endian = 0;

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    is_big_endian = 1;
#endif
#endif

    if (!backtrace_dwarf_add(self, base_address, &dwarf_sections, is_big_endian,
                             NULL, on_error, data, fileline_fn, NULL)) {
      goto fail;
    }
  }

  if (!have_dwarf && have_uuid) {
    if (!macho_add_dsym(self, filename, base_address, &uuid[0], on_error, data,
                        fileline_fn)) {
      goto fail;
    }
  }

  return 1;

fail:
  if (cmds_view_valid) {
    ten_mmap_deinit(&cmds_view);
  }
  if (descriptor != -1) {
    ten_backtrace_close_file(descriptor);
  }
  return 0;
}

// Initialize the backtrace data we need from a Mach-O executable using the dyld
// support functions.
int ten_backtrace_init_posix(
    ten_backtrace_t *self, const char *filename, int descriptor,
    ten_backtrace_on_error_func_t on_error, void *data,
    ten_backtrace_on_get_file_line_func_t *on_get_file_line) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  int closed_descriptor = 0;
  int found_sym = 0;
  ten_backtrace_on_get_file_line_func_t macho_fileline_fn = macho_nodebug;

  uint32_t c = _dyld_image_count();
  for (uint32_t i = 0; i < c; ++i) {
    uintptr_t base_address = 0;
    int d = 0;
    ten_backtrace_on_get_file_line_func_t mff = NULL;
    int mfs = 0;

    const char *name = _dyld_get_image_name(i);
    if (name == NULL) {
      continue;
    }

    if (strcmp(name, filename) == 0 && !closed_descriptor) {
      d = descriptor;
      closed_descriptor = 1;
    } else {
      bool does_not_exist = false;

      d = ten_backtrace_open_file(name, &does_not_exist);
      if (d < 0) {
        continue;
      }
    }

    base_address = _dyld_get_image_vmaddr_slide(i);

    mff = macho_nodebug;
    if (!macho_add(self, name, d, 0, NULL, base_address, 0, on_error, data,
                   &mff, &mfs)) {
      continue;
    }

    if (mff != macho_nodebug) {
      macho_fileline_fn = mff;
    }
    if (mfs) {
      found_sym = 1;
    }
  }

  if (!closed_descriptor) {
    ten_backtrace_close_file(descriptor);
  }

  if (found_sym) {
    ten_atomic_ptr_store((ten_atomic_ptr_t *)&self_posix->on_get_syminfo,
                         macho_syminfo);
  } else {
    (void)__sync_bool_compare_and_swap(&self_posix->on_get_syminfo, NULL,
                                       macho_nosyms);
  }

  *on_get_file_line =
      ten_atomic_ptr_load((ten_atomic_ptr_t *)&self_posix->on_get_file_line);

  if (*on_get_file_line == NULL || *on_get_file_line == macho_nodebug) {
    *on_get_file_line = macho_fileline_fn;
  }

  return 1;
}
