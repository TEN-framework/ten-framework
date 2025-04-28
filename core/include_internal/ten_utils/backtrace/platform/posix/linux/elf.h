//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/platform/posix/config.h"  // IWYU pragma: keep
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zlib.h"  // IWYU pragma: keep
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zstd.h"  // IWYU pragma: keep
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zutils.h"  // IWYU pragma: keep

// <link.h> might #include <elf.h> which might define our constants
// with slightly different values.  Undefine them to be safe.

#undef EI_NIDENT
#undef EI_MAG0
#undef EI_MAG1
#undef EI_MAG2
#undef EI_MAG3
#undef EI_CLASS
#undef EI_DATA
#undef EI_VERSION
#undef ELF_MAG0
#undef ELF_MAG1
#undef ELF_MAG2
#undef ELF_MAG3
#undef ELFCLASS32
#undef ELFCLASS64
#undef ELFDATA2LSB
#undef ELFDATA2MSB
#undef EV_CURRENT
#undef ET_DYN
#undef EM_PPC64
#undef EF_PPC64_ABI
#undef SHN_LORESERVE
#undef SHN_XINDEX
#undef SHN_UNDEF
#undef SHT_PROGBITS
#undef SHT_SYMTAB
#undef SHT_STRTAB
#undef SHT_DYNSYM
#undef SHF_COMPRESSED
#undef STT_OBJECT
#undef STT_FUNC
#undef NT_GNU_BUILD_ID
#undef ELFCOMPRESS_ZLIB
#undef ELFCOMPRESS_ZSTD

// The configure script must tell us whether we are 32-bit or 64-bit
// ELF.  We could make this code test and support either possibility,
// but there is no point.  This code only works for the currently
// running executable, which means that we know the ELF mode at
// configure time.

#if BACKTRACE_ELF_SIZE != 32 && BACKTRACE_ELF_SIZE != 64
#error "Unknown BACKTRACE_ELF_SIZE"
#endif

// Basic types.

typedef uint16_t b_elf_half;  // Elf_Half.
typedef uint32_t b_elf_word;  // Elf_Word.
typedef int32_t b_elf_sword;  // Elf_Sword.

#if BACKTRACE_ELF_SIZE == 32

typedef uint32_t b_elf_addr;  // Elf_Addr.
typedef uint32_t b_elf_off;   // Elf_Off.

typedef uint32_t b_elf_wxword;  // 32-bit Elf_Word, 64-bit ELF_Xword.

#else

typedef uint64_t b_elf_addr;   // Elf_Addr.
typedef uint64_t b_elf_off;    // Elf_Off.
typedef uint64_t b_elf_xword;  // Elf_Xword.
typedef int64_t b_elf_sxword;  // Elf_Sxword.

typedef uint64_t b_elf_wxword;  // 32-bit Elf_Word, 64-bit ELF_Xword.

#endif

// Data structures and associated constants.

#define EI_NIDENT 16

typedef struct {
  unsigned char e_ident[EI_NIDENT];  // ELF "magic number"
  b_elf_half e_type;                 // Identifies object file type
  b_elf_half e_machine;              // Specifies required architecture
  b_elf_word e_version;              // Identifies object file version
  b_elf_addr e_entry;                // Entry point virtual address
  b_elf_off e_phoff;                 // Program header table file offset
  b_elf_off e_shoff;                 // Section header table file offset
  b_elf_word e_flags;                // Processor-specific flags
  b_elf_half e_ehsize;               // ELF header size in bytes
  b_elf_half e_phentsize;            // Program header table entry size
  b_elf_half e_phnum;                // Program header table entry count
  b_elf_half e_shentsize;            // Section header table entry size
  b_elf_half e_shnum;                // Section header table entry count
  b_elf_half e_shstrndx;             // Section header string table index
} b_elf_ehdr;                        // Elf_Ehdr.

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EV_CURRENT 1

#define ET_DYN 3

#define EM_PPC64 21
#define EF_PPC64_ABI 3

typedef struct {
  b_elf_word sh_name;         // Section name, index in string tbl
  b_elf_word sh_type;         // Type of section
  b_elf_wxword sh_flags;      // Miscellaneous section attributes
  b_elf_addr sh_addr;         // Section virtual addr at execution
  b_elf_off sh_offset;        // Section file offset
  b_elf_wxword sh_size;       // Size of section in bytes
  b_elf_word sh_link;         // Index of another section
  b_elf_word sh_info;         // Additional section information
  b_elf_wxword sh_addralign;  // Section alignment
  b_elf_wxword sh_entsize;    // Entry size if section holds table
} b_elf_shdr;                 // Elf_Shdr.

#define SHN_UNDEF 0x0000      // Undefined section
#define SHN_LORESERVE 0xFF00  // Begin range of reserved indices
#define SHN_XINDEX 0xFFFF     // Section index is held elsewhere

#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_DYNSYM 11

#define SHF_COMPRESSED 0x800

#if BACKTRACE_ELF_SIZE == 32

typedef struct {
  b_elf_word st_name;      // Symbol name, index in string tbl
  b_elf_addr st_value;     // Symbol value
  b_elf_word st_size;      // Symbol size
  unsigned char st_info;   // Symbol binding and type
  unsigned char st_other;  // Visibility and other data
  b_elf_half st_shndx;     // Symbol section index
} b_elf_sym;               // Elf_Sym.

#else  // BACKTRACE_ELF_SIZE != 32

typedef struct {
  b_elf_word st_name;      // Symbol name, index in string tbl
  unsigned char st_info;   // Symbol binding and type
  unsigned char st_other;  // Visibility and other data
  b_elf_half st_shndx;     // Symbol section index
  b_elf_addr st_value;     // Symbol value
  b_elf_xword st_size;     // Symbol size
} b_elf_sym;               // Elf_Sym.

#endif  // BACKTRACE_ELF_SIZE != 32

#define STT_OBJECT 1
#define STT_FUNC 2

typedef struct {
  uint32_t namesz;
  uint32_t descsz;
  uint32_t type;
  char name[1];
} b_elf_note;

#define NT_GNU_BUILD_ID 3

#if BACKTRACE_ELF_SIZE == 32

typedef struct {
  b_elf_word ch_type;       // Compresstion algorithm
  b_elf_word ch_size;       // Uncompressed size
  b_elf_word ch_addralign;  // Alignment for uncompressed data
} b_elf_chdr;               // Elf_Chdr

#else  // BACKTRACE_ELF_SIZE != 32

typedef struct {
  b_elf_word ch_type;        // Compression algorithm
  b_elf_word ch_reserved;    // Reserved
  b_elf_xword ch_size;       // Uncompressed size
  b_elf_xword ch_addralign;  // Alignment for uncompressed data
} b_elf_chdr;                // Elf_Chdr

#endif  // BACKTRACE_ELF_SIZE != 32

#define ELFCOMPRESS_ZLIB 1
#define ELFCOMPRESS_ZSTD 2

// Names of sections, indexed by enum dwarf_section in internal.h.
static const char *const dwarf_section_names[DEBUG_MAX] = {
    ".debug_info",        ".debug_line",     ".debug_abbrev",
    ".debug_ranges",      ".debug_str",      ".debug_addr",
    ".debug_str_offsets", ".debug_line_str", ".debug_rnglists"};

// Information we gather for the sections we care about.
typedef struct debug_section_info {
  // Section file offset.
  off_t offset;
  // Section size.
  size_t size;
  // Section contents, after read from file.
  const unsigned char *data;
  // Whether the SHF_COMPRESSED flag is set for the section.
  int compressed;
} debug_section_info;
