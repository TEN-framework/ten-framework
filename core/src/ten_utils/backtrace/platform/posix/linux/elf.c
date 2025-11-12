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

#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"
#include "include_internal/ten_utils/backtrace/platform/posix/file.h"
#include "include_internal/ten_utils/backtrace/platform/posix/internal.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/debugfile.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/lzma.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/symbol.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zdebug.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/zlib.h"
#include "include_internal/ten_utils/backtrace/platform/posix/mmap.h"
#include "ten_utils/lib/atomic_ptr.h"

#if defined(HAVE_DL_ITERATE_PHDR)
#if defined(HAVE_LINK_H)
#define __USE_GNU
#include <link.h>
#undef __USE_GNU
#endif
#endif

#ifndef S_ISLNK
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_IFMT
#define S_IFMT 0170000
#endif
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#ifndef HAVE_DL_ITERATE_PHDR

// Dummy version of dl_iterate_phdr for systems that don't have it.

#define dl_phdr_info x_dl_phdr_info
#define dl_iterate_phdr x_dl_iterate_phdr

typedef struct dl_phdr_info {
  uintptr_t dlpi_addr;
  const char *dlpi_name;
} dl_phdr_info;

static int dl_iterate_phdr(int (*callback)(dl_phdr_info *, size_t, void *),
                           void *data) {
  return 0;
}

#endif  // ! defined (HAVE_DL_ITERATE_PHDR)

/**
 * @brief A dummy callback function used when no symbol table is available.
 *
 * This function is called when we can't find a symbol table in an ELF
 * executable. It simply reports an error through the provided error callback
 * and doesn't attempt to look up any symbol information.
 *
 * @param self The backtrace context.
 * @param pc The program counter address for which symbol information is
 * requested.
 * @param on_dump_syminfo Callback function to report symbol information.
 * @param on_error Function to call to report errors.
 * @param data User data to pass to the error callback.
 */
static void elf_nosyms(ten_backtrace_t *self, TEN_UNUSED uintptr_t pc,
                       TEN_UNUSED ten_backtrace_on_dump_syminfo_func_t
                           on_dump_syminfo,
                       ten_backtrace_on_error_func_t on_error, void *data) {
  on_error(self, "no symbol table in ELF executable", -1, data);
}

/**
 * @brief A callback function used when no debug information is available.
 *
 * This function is called when we can't find DWARF debug information for an ELF
 * file. It attempts to fall back to symbol table information if available,
 * which can at least provide function names even without line number
 * information.
 *
 * @param self The backtrace context.
 * @param pc The program counter address to look up.
 * @param on_dump_file_line Function to call with file/line information.
 * @param on_error Function to call to report errors.
 * @param data User data to pass to callbacks.
 * @return 1 if symbol information was found and callback was called, 0
 * otherwise.
 */
static int elf_nodebug(ten_backtrace_t *self, uintptr_t pc,
                       ten_backtrace_on_dump_file_line_func_t on_dump_file_line,
                       ten_backtrace_on_error_func_t on_error, void *data) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  // If we have a symbol table but no debug info, we can at least report
  // function names.
  if (self_posix->on_get_syminfo != NULL &&
      self_posix->on_get_syminfo != elf_nosyms) {
    backtrace_call_full bt_data;

    // Initialize the adapter structure to convert symbol info to file/line
    // format.
    bt_data.on_dump_file_line = on_dump_file_line;
    bt_data.on_error = on_error;
    bt_data.data = data;
    bt_data.ret = 0;

    // Call the symbol lookup function with our adapter callbacks.
    self_posix->on_get_syminfo(self, pc, backtrace_dump_syminfo_to_file_line,
                               backtrace_dump_syminfo_to_file_line_error,
                               &bt_data);

    return bt_data.ret;
  }

  // Report that no debug information is available.
  on_error(self, "no debug info in ELF executable", -1, data);

  return 0;
}

/**
 * @brief Add the backtrace data for one ELF file.
 *
 * @return 1 on success, 0 on failure (in both cases descriptor is closed) or -1
 * if exe is non-zero and the ELF file is ET_DYN, which tells the caller that
 * elf_add will need to be called on the descriptor again after base_address is
 * determined.
 */
static int elf_add(ten_backtrace_t *self, const char *filename, int descriptor,
                   const unsigned char *memory, size_t memory_size,
                   uintptr_t base_address,
                   ten_backtrace_on_error_func_t on_error, void *data,
                   ten_backtrace_on_get_file_line_func_t *on_get_file_line,
                   int *found_sym, int *found_dwarf,
                   dwarf_data **fileline_entry, int exe, int debuginfo,
                   const char *with_buildid_data, uint32_t with_buildid_size) {
  elf_view ehdr_view;
  b_elf_ehdr ehdr;
  off_t shoff = 0;
  unsigned int shnum = 0;
  unsigned int shstrndx = 0;
  elf_view shdrs_view;
  int shdrs_view_valid = 0;
  const b_elf_shdr *shdrs = NULL;
  const b_elf_shdr *shstrhdr = NULL;
  size_t shstr_size = 0;
  off_t shstr_off = 0;
  elf_view names_view;
  int names_view_valid = 0;
  const char *names = NULL;
  unsigned int symtab_shndx = 0;
  unsigned int dynsym_shndx = 0;
  unsigned int i = 0;
  debug_section_info sections[DEBUG_MAX];
  debug_section_info zsections[DEBUG_MAX];
  elf_view symtab_view;
  int symtab_view_valid = 0;
  elf_view strtab_view;
  int strtab_view_valid = 0;
  elf_view buildid_view;
  int buildid_view_valid = 0;
  const char *buildid_data = NULL;
  uint32_t buildid_size = 0;
  elf_view debuglink_view;
  int debuglink_view_valid = 0;
  const char *debuglink_name = NULL;
  uint32_t debuglink_crc = 0;
  elf_view debugaltlink_view;
  int debugaltlink_view_valid = 0;
  const char *debugaltlink_name = NULL;
  const char *debugaltlink_buildid_data = NULL;
  uint32_t debugaltlink_buildid_size = 0;
  elf_view gnu_debugdata_view;
  int gnu_debugdata_view_valid = 0;
  size_t gnu_debugdata_size = 0;
  unsigned char *gnu_debugdata_uncompressed = NULL;
  size_t gnu_debugdata_uncompressed_size = 0;
  off_t min_offset = 0;
  off_t max_offset = 0;
  off_t debug_size = 0;
  elf_view debug_view;
  int debug_view_valid = 0;
  unsigned int using_debug_view = 0;
  uint16_t *zdebug_table = NULL;
  elf_view split_debug_view[DEBUG_MAX];
  unsigned char split_debug_view_valid[DEBUG_MAX];
  elf_ppc64_opd_data opd_data;
  elf_ppc64_opd_data *opd = NULL;
  dwarf_sections dwarf_sections;

  if (!debuginfo) {
    *found_sym = 0;
    *found_dwarf = 0;
  }

  shdrs_view_valid = 0;
  names_view_valid = 0;
  symtab_view_valid = 0;
  strtab_view_valid = 0;
  buildid_view_valid = 0;
  buildid_data = NULL;
  buildid_size = 0;
  debuglink_view_valid = 0;
  debuglink_name = NULL;
  debuglink_crc = 0;
  debugaltlink_view_valid = 0;
  debugaltlink_name = NULL;
  debugaltlink_buildid_data = NULL;
  debugaltlink_buildid_size = 0;
  gnu_debugdata_view_valid = 0;
  gnu_debugdata_size = 0;
  debug_view_valid = 0;
  memset(&split_debug_view_valid[0], 0, sizeof split_debug_view_valid);
  opd = NULL;

  if (!elf_get_view(self, descriptor, memory, memory_size, 0, sizeof ehdr,
                    on_error, data, &ehdr_view)) {
    goto fail;
  }

  memcpy(&ehdr, ehdr_view.view.data, sizeof ehdr);

  elf_release_view(self, &ehdr_view, on_error, data);

  if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
      ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3) {
    on_error(self, "executable file is not ELF", 0, data);
    goto fail;
  }
  if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
    on_error(self, "executable file is unrecognized ELF version", 0, data);
    goto fail;
  }

#if BACKTRACE_ELF_SIZE == 32
#define BACKTRACE_ELFCLASS ELFCLASS32
#else
#define BACKTRACE_ELFCLASS ELFCLASS64
#endif

  if (ehdr.e_ident[EI_CLASS] != BACKTRACE_ELFCLASS) {
    on_error(self, "executable file is unexpected ELF class", 0, data);
    goto fail;
  }

  if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB &&
      ehdr.e_ident[EI_DATA] != ELFDATA2MSB) {
    on_error(self, "executable file has unknown endianness", 0, data);
    goto fail;
  }

  // If the executable is ET_DYN, it is either a PIE, or we are running
  // directly a shared library with .interp.  We need to wait for
  // dl_iterate_phdr in that case to determine the actual base_address.
  if (exe && ehdr.e_type == ET_DYN) {
    return -1;
  }

  shoff = ehdr.e_shoff;
  shnum = ehdr.e_shnum;
  shstrndx = ehdr.e_shstrndx;

  if ((shnum == 0 || shstrndx == SHN_XINDEX) && shoff != 0) {
    elf_view shdr_view;
    const b_elf_shdr *shdr = NULL;

    if (!elf_get_view(self, descriptor, memory, memory_size, shoff,
                      sizeof(shdr), on_error, data, &shdr_view)) {
      goto fail;
    }

    shdr = (const b_elf_shdr *)shdr_view.view.data;

    if (shnum == 0) {
      shnum = shdr->sh_size;
    }

    if (shstrndx == SHN_XINDEX) {
      shstrndx = shdr->sh_link;

      // Versions of the GNU binutils between 2.12 and 2.18 did
      // not handle objects with more than SHN_LORESERVE sections
      // correctly.  All large section indexes were offset by
      // 0x100.  There is more information at
      // http://sourceware.org/bugzilla/show_bug.cgi?id-5900 .
      // Fortunately these object files are easy to detect, as the
      // GNU binutils always put the section header string table
      // near the end of the list of sections.  Thus if the
      // section header string table index is larger than the
      // number of sections, then we know we have to subtract
      // 0x100 to get the real section index.
      if (shstrndx >= shnum && shstrndx >= SHN_LORESERVE + 0x100) {
        shstrndx -= 0x100;
      }
    }

    elf_release_view(self, &shdr_view, on_error, data);
  }

  if (shnum == 0 || shstrndx == 0) {
    goto fail;
  }

  // To translate PC to file/line when using DWARF, we need to find
  // the .debug_info and .debug_line sections.

  // Read the section headers, skipping the first one.

  if (!elf_get_view(
          self, descriptor, memory, memory_size, shoff + sizeof(b_elf_shdr),
          (shnum - 1) * sizeof(b_elf_shdr), on_error, data, &shdrs_view)) {
    goto fail;
  }
  shdrs_view_valid = 1;
  shdrs = (const b_elf_shdr *)shdrs_view.view.data;

  // Read the section names.

  shstrhdr = &shdrs[shstrndx - 1];
  shstr_size = shstrhdr->sh_size;
  shstr_off = shstrhdr->sh_offset;

  if (!elf_get_view(self, descriptor, memory, memory_size, shstr_off,
                    shstrhdr->sh_size, on_error, data, &names_view)) {
    goto fail;
  }
  names_view_valid = 1;
  names = (const char *)names_view.view.data;

  symtab_shndx = 0;
  dynsym_shndx = 0;

  memset(sections, 0, sizeof sections);
  memset(zsections, 0, sizeof zsections);

  // Look for the symbol table.
  for (i = 1; i < shnum; ++i) {
    const b_elf_shdr *shdr = NULL;
    unsigned int sh_name = 0;
    const char *name = NULL;
    int j = 0;

    shdr = &shdrs[i - 1];

    if (shdr->sh_type == SHT_SYMTAB) {
      symtab_shndx = i;
    } else if (shdr->sh_type == SHT_DYNSYM) {
      dynsym_shndx = i;
    }

    sh_name = shdr->sh_name;
    if (sh_name >= shstr_size) {
      on_error(self, "ELF section name out of range", 0, data);
      goto fail;
    }

    name = names + sh_name;

    for (j = 0; j < DEBUG_MAX; ++j) {
      if (strcmp(name, dwarf_section_names[j]) == 0) {
        sections[j].offset = shdr->sh_offset;
        sections[j].size = shdr->sh_size;
        sections[j].compressed = (shdr->sh_flags & SHF_COMPRESSED) != 0;
        break;
      }
    }

    if (name[0] == '.' && name[1] == 'z') {
      for (j = 0; j < DEBUG_MAX; ++j) {
        if (strcmp(name + 2, dwarf_section_names[j] + 1) == 0) {
          zsections[j].offset = shdr->sh_offset;
          zsections[j].size = shdr->sh_size;
          break;
        }
      }
    }

    // Read the build ID if present.  This could check for any
    // SHT_NOTE section with the right note name and type, but gdb
    // looks for a specific section name.
    if ((!debuginfo || with_buildid_data != NULL) && !buildid_view_valid &&
        strcmp(name, ".note.gnu.build-id") == 0) {
      const b_elf_note *note = NULL;

      if (!elf_get_view(self, descriptor, memory, memory_size, shdr->sh_offset,
                        shdr->sh_size, on_error, data, &buildid_view)) {
        goto fail;
      }

      buildid_view_valid = 1;
      note = (const b_elf_note *)buildid_view.view.data;
      if (note->type == NT_GNU_BUILD_ID && note->namesz == 4 &&
          strncmp(note->name, "GNU", 4) == 0 &&
          shdr->sh_size <= 12 + ((note->namesz + 3) & ~3) + note->descsz) {
        buildid_data = &note->name[0] + ((note->namesz + 3) & ~3);
        buildid_size = note->descsz;
      }

      if (with_buildid_size != 0) {
        if (buildid_size != with_buildid_size) {
          goto fail;
        }

        if (memcmp(buildid_data, with_buildid_data, buildid_size) != 0) {
          goto fail;
        }
      }
    }

    // Read the debuglink file if present.
    if (!debuginfo && !debuglink_view_valid &&
        strcmp(name, ".gnu_debuglink") == 0) {
      const char *debuglink_data = NULL;
      size_t crc_offset = 0;

      if (!elf_get_view(self, descriptor, memory, memory_size, shdr->sh_offset,
                        shdr->sh_size, on_error, data, &debuglink_view)) {
        goto fail;
      }

      debuglink_view_valid = 1;
      debuglink_data = (const char *)debuglink_view.view.data;
      crc_offset = strnlen(debuglink_data, shdr->sh_size);
      crc_offset = (crc_offset + 3) & ~3;
      if (crc_offset + 4 <= shdr->sh_size) {
        debuglink_name = debuglink_data;
        debuglink_crc = *(const uint32_t *)(debuglink_data + crc_offset);
      }
    }

    if (!debugaltlink_view_valid && strcmp(name, ".gnu_debugaltlink") == 0) {
      const char *debugaltlink_data = NULL;
      size_t debugaltlink_name_len = 0;

      if (!elf_get_view(self, descriptor, memory, memory_size, shdr->sh_offset,
                        shdr->sh_size, on_error, data, &debugaltlink_view)) {
        goto fail;
      }

      debugaltlink_view_valid = 1;
      debugaltlink_data = (const char *)debugaltlink_view.view.data;
      debugaltlink_name = debugaltlink_data;
      debugaltlink_name_len = strnlen(debugaltlink_data, shdr->sh_size);
      if (debugaltlink_name_len < shdr->sh_size) {
        // Include terminating zero.
        debugaltlink_name_len += 1;

        debugaltlink_buildid_data = debugaltlink_data + debugaltlink_name_len;
        debugaltlink_buildid_size = shdr->sh_size - debugaltlink_name_len;
      }
    }

    if (!gnu_debugdata_view_valid && strcmp(name, ".gnu_debugdata") == 0) {
      if (!elf_get_view(self, descriptor, memory, memory_size, shdr->sh_offset,
                        shdr->sh_size, on_error, data, &gnu_debugdata_view)) {
        goto fail;
      }

      gnu_debugdata_size = shdr->sh_size;
      gnu_debugdata_view_valid = 1;
    }

    // Read the .opd section on PowerPC64 ELFv1.
    if (ehdr.e_machine == EM_PPC64 && (ehdr.e_flags & EF_PPC64_ABI) < 2 &&
        shdr->sh_type == SHT_PROGBITS && strcmp(name, ".opd") == 0) {
      if (!elf_get_view(self, descriptor, memory, memory_size, shdr->sh_offset,
                        shdr->sh_size, on_error, data, &opd_data.view)) {
        goto fail;
      }

      opd = &opd_data;
      opd->addr = shdr->sh_addr;
      opd->data = (const char *)opd_data.view.view.data;
      opd->size = shdr->sh_size;
    }
  }

  if (symtab_shndx == 0) {
    symtab_shndx = dynsym_shndx;
  }

  if (symtab_shndx != 0 && !debuginfo) {
    const b_elf_shdr *symtab_shdr = NULL;
    unsigned int strtab_shndx = 0;
    const b_elf_shdr *strtab_shdr = NULL;
    elf_syminfo_data *sdata = NULL;

    symtab_shdr = &shdrs[symtab_shndx - 1];
    strtab_shndx = symtab_shdr->sh_link;
    if (strtab_shndx >= shnum) {
      on_error(self, "ELF symbol table strtab link out of range", 0, data);
      goto fail;
    }
    strtab_shdr = &shdrs[strtab_shndx - 1];

    if (!elf_get_view(self, descriptor, memory, memory_size,
                      symtab_shdr->sh_offset, symtab_shdr->sh_size, on_error,
                      data, &symtab_view)) {
      goto fail;
    }
    symtab_view_valid = 1;

    if (!elf_get_view(self, descriptor, memory, memory_size,
                      strtab_shdr->sh_offset, strtab_shdr->sh_size, on_error,
                      data, &strtab_view)) {
      goto fail;
    }
    strtab_view_valid = 1;

    sdata = (elf_syminfo_data *)malloc(sizeof *sdata);
    if (sdata == NULL) {
      goto fail;
    }

    if (!elf_initialize_syminfo(self, base_address, symtab_view.view.data,
                                symtab_shdr->sh_size, strtab_view.view.data,
                                strtab_shdr->sh_size, on_error, data, sdata,
                                opd)) {
      free(sdata);
      goto fail;
    }

    // We no longer need the symbol table, but we hold on to the
    // string table permanently.
    elf_release_view(self, &symtab_view, on_error, data);
    symtab_view_valid = 0;
    strtab_view_valid = 0;

    *found_sym = 1;

    elf_add_syminfo_data(self, sdata);
  }

  elf_release_view(self, &shdrs_view, on_error, data);
  shdrs_view_valid = 0;
  elf_release_view(self, &names_view, on_error, data);
  names_view_valid = 0;

  // If the debug info is in a separate file, read that one instead.

  if (buildid_data != NULL) {
    int d = elf_open_debug_file_by_build_id(self, buildid_data, buildid_size);
    if (d >= 0) {
      int ret = 0;

      elf_release_view(self, &buildid_view, on_error, data);
      if (debuglink_view_valid) {
        elf_release_view(self, &debuglink_view, on_error, data);
      }
      if (debugaltlink_view_valid) {
        elf_release_view(self, &debugaltlink_view, on_error, data);
      }

      ret = elf_add(self, "", d, NULL, 0, base_address, on_error, data,
                    on_get_file_line, found_sym, found_dwarf, NULL, 0, 1, NULL,
                    0);
      if (ret < 0) {
        ten_backtrace_close_file(d);
      } else if (descriptor >= 0) {
        ten_backtrace_close_file(descriptor);
      }

      return ret;
    }
  }

  if (buildid_view_valid) {
    elf_release_view(self, &buildid_view, on_error, data);
    buildid_view_valid = 0;
  }

  if (opd) {
    elf_release_view(self, &opd->view, on_error, data);
    opd = NULL;
  }

  if (debuglink_name != NULL) {
    int d = 0;

    d = elf_open_debug_file_by_debug_link(self, filename, debuglink_name,
                                          debuglink_crc, on_error, data);
    if (d >= 0) {
      int ret = 0;

      elf_release_view(self, &debuglink_view, on_error, data);

      if (debugaltlink_view_valid) {
        elf_release_view(self, &debugaltlink_view, on_error, data);
      }

      ret = elf_add(self, "", d, NULL, 0, base_address, on_error, data,
                    on_get_file_line, found_sym, found_dwarf, NULL, 0, 1, NULL,
                    0);
      if (ret < 0) {
        ten_backtrace_close_file(d);
      } else if (descriptor >= 0) {
        ten_backtrace_close_file(descriptor);
      }

      return ret;
    }
  }

  if (debuglink_view_valid) {
    elf_release_view(self, &debuglink_view, on_error, data);
    debuglink_view_valid = 0;
  }

  dwarf_data *fileline_altlink = NULL;
  if (debugaltlink_name != NULL) {
    int d = elf_open_debug_file_by_debug_link(self, filename, debugaltlink_name,
                                              0, on_error, data);
    if (d >= 0) {
      int ret =
          elf_add(self, filename, d, NULL, 0, base_address, on_error, data,
                  on_get_file_line, found_sym, found_dwarf, &fileline_altlink,
                  0, 1, debugaltlink_buildid_data, debugaltlink_buildid_size);
      elf_release_view(self, &debugaltlink_view, on_error, data);
      debugaltlink_view_valid = 0;
      if (ret < 0) {
        ten_backtrace_close_file(d);
        return ret;
      }
    }
  }

  if (debugaltlink_view_valid) {
    elf_release_view(self, &debugaltlink_view, on_error, data);
    debugaltlink_view_valid = 0;
  }

  if (gnu_debugdata_view_valid) {
    int ret = elf_uncompress_lzma(
        self, ((const unsigned char *)gnu_debugdata_view.view.data),
        gnu_debugdata_size, on_error, data, &gnu_debugdata_uncompressed,
        &gnu_debugdata_uncompressed_size);

    elf_release_view(self, &gnu_debugdata_view, on_error, data);
    gnu_debugdata_view_valid = 0;

    if (ret) {
      ret = elf_add(self, filename, -1, gnu_debugdata_uncompressed,
                    gnu_debugdata_uncompressed_size, base_address, on_error,
                    data, on_get_file_line, found_sym, found_dwarf, NULL, 0, 0,
                    NULL, 0);
      if (ret >= 0 && descriptor >= 0) {
        ten_backtrace_close_file(descriptor);
      }
      return ret;
    }
  }

  // Read all the debug sections in a single view, since they are
  // probably adjacent in the file.  If any of sections are
  // uncompressed, we never release this view.

  min_offset = 0;
  max_offset = 0;
  debug_size = 0;
  for (i = 0; i < DEBUG_MAX; ++i) {
    off_t end = 0;

    if (sections[i].size != 0) {
      if (min_offset == 0 || sections[i].offset < min_offset) {
        min_offset = sections[i].offset;
      }
      end = sections[i].offset + sections[i].size;
      if (end > max_offset) {
        max_offset = end;
      }
      debug_size += sections[i].size;
    }
    if (zsections[i].size != 0) {
      if (min_offset == 0 || zsections[i].offset < min_offset) {
        min_offset = zsections[i].offset;
      }
      end = zsections[i].offset + zsections[i].size;
      if (end > max_offset) {
        max_offset = end;
      }
      debug_size += zsections[i].size;
    }
  }
  if (min_offset == 0 || max_offset == 0) {
    if (descriptor >= 0) {
      if (!ten_backtrace_close_file(descriptor)) {
        goto fail;
      }
    }
    return 1;
  }

  // If the total debug section size is large, assume that there are
  // gaps between the sections, and read them individually.

  if (max_offset - min_offset < 0x20000000 ||
      max_offset - min_offset < debug_size + 0x10000) {
    if (!elf_get_view(self, descriptor, memory, memory_size, min_offset,
                      max_offset - min_offset, on_error, data, &debug_view)) {
      goto fail;
    }
    debug_view_valid = 1;
  } else {
    memset(&split_debug_view[0], 0, sizeof split_debug_view);
    for (i = 0; i < DEBUG_MAX; ++i) {
      debug_section_info *dsec = NULL;

      if (sections[i].size != 0) {
        dsec = &sections[i];
      } else if (zsections[i].size != 0) {
        dsec = &zsections[i];
      } else {
        continue;
      }

      if (!elf_get_view(self, descriptor, memory, memory_size, dsec->offset,
                        dsec->size, on_error, data, &split_debug_view[i])) {
        goto fail;
      }
      split_debug_view_valid[i] = 1;

      if (sections[i].size != 0) {
        sections[i].data =
            ((const unsigned char *)split_debug_view[i].view.data);
      } else {
        zsections[i].data =
            ((const unsigned char *)split_debug_view[i].view.data);
      }
    }
  }

  // We've read all we need from the executable.
  if (descriptor >= 0) {
    if (!ten_backtrace_close_file(descriptor)) {
      goto fail;
    }
    descriptor = -1;
  }

  using_debug_view = 0;
  if (debug_view_valid) {
    for (i = 0; i < DEBUG_MAX; ++i) {
      if (sections[i].size == 0) {
        sections[i].data = NULL;
      } else {
        sections[i].data = ((const unsigned char *)debug_view.view.data +
                            (sections[i].offset - min_offset));
        ++using_debug_view;
      }

      if (zsections[i].size == 0) {
        zsections[i].data = NULL;
      } else {
        zsections[i].data = ((const unsigned char *)debug_view.view.data +
                             (zsections[i].offset - min_offset));
      }
    }
  }

  // Uncompress the old format (--compress-debug-sections=zlib-gnu).

  zdebug_table = NULL;
  for (i = 0; i < DEBUG_MAX; ++i) {
    if (sections[i].size == 0 && zsections[i].size > 0) {
      unsigned char *uncompressed_data = NULL;
      size_t uncompressed_size = 0;

      if (zdebug_table == NULL) {
        zdebug_table = (uint16_t *)malloc(ZLIB_TABLE_SIZE);
        if (zdebug_table == NULL) {
          goto fail;
        }
      }

      uncompressed_data = NULL;
      uncompressed_size = 0;
      if (!elf_uncompress_zdebug(self, zsections[i].data, zsections[i].size,
                                 zdebug_table, on_error, data,
                                 &uncompressed_data, &uncompressed_size)) {
        goto fail;
      }
      sections[i].data = uncompressed_data;
      sections[i].size = uncompressed_size;
      sections[i].compressed = 0;

      if (split_debug_view_valid[i]) {
        elf_release_view(self, &split_debug_view[i], on_error, data);
        split_debug_view_valid[i] = 0;
      }
    }
  }

  if (zdebug_table != NULL) {
    free(zdebug_table);
    zdebug_table = NULL;
  }

  // Uncompress the official ELF format
  // (--compress-debug-sections=zlib-gabi, --compress-debug-sections=zstd).
  for (i = 0; i < DEBUG_MAX; ++i) {
    unsigned char *uncompressed_data = NULL;
    size_t uncompressed_size = 0;

    if (sections[i].size == 0 || !sections[i].compressed) {
      continue;
    }

    if (zdebug_table == NULL) {
      zdebug_table = (uint16_t *)malloc(ZDEBUG_TABLE_SIZE);
      if (zdebug_table == NULL) {
        goto fail;
      }
    }

    uncompressed_data = NULL;
    uncompressed_size = 0;
    if (!elf_uncompress_chdr(self, sections[i].data, sections[i].size,
                             zdebug_table, on_error, data, &uncompressed_data,
                             &uncompressed_size)) {
      goto fail;
    }

    sections[i].data = uncompressed_data;
    sections[i].size = uncompressed_size;
    sections[i].compressed = 0;

    if (debug_view_valid) {
      --using_debug_view;
    } else if (split_debug_view_valid[i]) {
      elf_release_view(self, &split_debug_view[i], on_error, data);
      split_debug_view_valid[i] = 0;
    }
  }

  if (zdebug_table != NULL) {
    free(zdebug_table);
  }

  if (debug_view_valid && using_debug_view == 0) {
    elf_release_view(self, &debug_view, on_error, data);
    debug_view_valid = 0;
  }

  for (i = 0; i < DEBUG_MAX; ++i) {
    dwarf_sections.data[i] = sections[i].data;
    dwarf_sections.size[i] = sections[i].size;
  }

  if (!backtrace_dwarf_add(self, base_address, &dwarf_sections,
                           ehdr.e_ident[EI_DATA] == ELFDATA2MSB,
                           fileline_altlink, on_error, data, on_get_file_line,
                           fileline_entry)) {
    goto fail;
  }

  *found_dwarf = 1;

  return 1;

fail:
  if (shdrs_view_valid) {
    elf_release_view(self, &shdrs_view, on_error, data);
  }
  if (names_view_valid) {
    elf_release_view(self, &names_view, on_error, data);
  }
  if (symtab_view_valid) {
    elf_release_view(self, &symtab_view, on_error, data);
  }
  if (strtab_view_valid) {
    elf_release_view(self, &strtab_view, on_error, data);
  }
  if (debuglink_view_valid) {
    elf_release_view(self, &debuglink_view, on_error, data);
  }
  if (debugaltlink_view_valid) {
    elf_release_view(self, &debugaltlink_view, on_error, data);
  }
  if (gnu_debugdata_view_valid) {
    elf_release_view(self, &gnu_debugdata_view, on_error, data);
  }
  if (buildid_view_valid) {
    elf_release_view(self, &buildid_view, on_error, data);
  }
  if (debug_view_valid) {
    elf_release_view(self, &debug_view, on_error, data);
  }
  for (i = 0; i < DEBUG_MAX; ++i) {
    if (split_debug_view_valid[i]) {
      elf_release_view(self, &split_debug_view[i], on_error, data);
    }
  }
  if (opd) {
    elf_release_view(self, &opd->view, on_error, data);
  }
  if (descriptor >= 0) {
    ten_backtrace_close_file(descriptor);
  }
  return 0;
}

/**
 * @brief Structure containing data passed to the `phdr_callback` function.
 *
 * This structure holds all the necessary context for processing program headers
 * during shared library iteration with `dl_iterate_phdr()`. It maintains
 * references to the backtrace context, error handling callbacks, and tracks the
 * state of symbol and DWARF debug information discovery.
 */
typedef struct phdr_data {
  ten_backtrace_t *backtrace;              // Backtrace context.
  ten_backtrace_on_error_func_t on_error;  // Error callback function.
  void *data;                              // User data for callbacks.

  // Output parameter for file/line lookup function.
  ten_backtrace_on_get_file_line_func_t *on_get_file_line;

  // Output flags indicating if symbols and DWARF debug info were found.
  int *found_sym;
  int *found_dwarf;

  // Path to the executable file.
  const char *exe_filename;

  // File descriptor for the executable, or -1 if already used.
  int exe_descriptor;
} phdr_data;

/**
 * @brief Callback function passed to `dl_iterate_phdr` to load debug info from
 * shared libraries.
 *
 * This function is called for each loaded shared object in the process. It
 * attempts to extract debug information from the shared library and updates the
 * backtrace context with this information.
 *
 * For the main executable (which may have a NULL or empty name in the
 * `dl_phdr_info`), it uses the pre-opened descriptor. For other shared
 * libraries, it opens the file specified by dlpi_name.
 *
 * @param info Pointer to structure containing information about the shared
 * object.
 * @param size Size of the structure (for versioning).
 * @param pdata Pointer to user data (phdr_data structure).
 * @return 0 to continue iteration, non-zero to stop.
 */
static int
#if defined(__i386__)
    __attribute__((__force_align_arg_pointer__))
#endif
    phdr_callback(struct dl_phdr_info *info, TEN_UNUSED size_t size,
                  void *pdata) {
  phdr_data *pd = (phdr_data *)pdata;
  const char *filename = NULL;
  int descriptor = 0;
  bool does_not_exist = false;
  ten_backtrace_on_get_file_line_func_t on_get_file_line = NULL;
  int found_dwarf = 0;

  // Handle the main executable (NULL or empty name) or shared libraries.
  //
  // There is not much we can do if we don't have the module name, unless
  // executable is ET_DYN, where we expect the very first phdr_callback to be
  // for the PIE.
  if (info->dlpi_name == NULL || info->dlpi_name[0] == '\0') {
    // This is likely the main executable.
    if (pd->exe_descriptor == -1) {
      // Descriptor already used or invalid.
      return 0;
    }

    filename = pd->exe_filename;
    descriptor = pd->exe_descriptor;
    pd->exe_descriptor = -1;  // Mark as used.
  } else {
    // This is a shared library.
    // Close the executable descriptor if it's still open.
    if (pd->exe_descriptor != -1) {
      ten_backtrace_close_file(pd->exe_descriptor);
      pd->exe_descriptor = -1;
    }

    filename = info->dlpi_name;
    descriptor = ten_backtrace_open_file(info->dlpi_name, &does_not_exist);
    if (descriptor < 0) {
      // Failed to open the file.
      return 0;
    }
  }

  // Try to extract debug information from the file.
  if (elf_add(pd->backtrace, filename, descriptor, NULL, 0, info->dlpi_addr,
              pd->on_error, pd->data, &on_get_file_line, pd->found_sym,
              &found_dwarf, NULL, 0, 0, NULL, 0)) {
    // If we found DWARF debug info, update the function pointer and flag.
    if (found_dwarf) {
      *pd->found_dwarf = 1;
      *pd->on_get_file_line = on_get_file_line;
    }
  } else {
    // elf_add failed but we still need to close the descriptor if it's a shared
    // library.
    if (info->dlpi_name != NULL && info->dlpi_name[0] != '\0') {
      ten_backtrace_close_file(descriptor);
    }
  }

  return 0;  // Continue iteration.
}

/**
 * @brief Initialize the backtrace data we need from an ELF executable.
 *
 * This function attempts to extract debug information from an ELF executable
 * file. It first tries to process the specified file directly, then iterates
 * through all loaded shared objects using `dl_iterate_phdr` to gather
 * additional debug information.
 *
 * The function sets up the appropriate function pointers for symbol lookup and
 * file/line information based on the available debug data in the executable and
 * its dependencies.
 *
 * @param self The backtrace context.
 * @param filename Path to the executable file.
 * @param descriptor Open file descriptor for the executable.
 * @param on_error Callback function for reporting errors.
 * @param data User data to pass to the error callback.
 * @param on_get_file_line Pointer to store the file/line lookup function.
 * @return 1 on success, 0 on failure.
 */
int ten_backtrace_init_posix(
    ten_backtrace_t *self, const char *filename, int descriptor,
    ten_backtrace_on_error_func_t on_error, void *data,
    ten_backtrace_on_get_file_line_func_t *on_get_file_line) {
  ten_backtrace_posix_t *self_posix = (ten_backtrace_posix_t *)self;
  assert(self_posix && "Invalid argument.");

  int ret = 0;
  int found_sym = 0;
  int found_dwarf = 0;
  ten_backtrace_on_get_file_line_func_t on_get_file_line_ = elf_nodebug;

  // First, try to process the main executable file.
  ret = elf_add(self, filename, descriptor, NULL, 0, 0, on_error, data,
                &on_get_file_line_, &found_sym, &found_dwarf, NULL, 1, 0, NULL,
                0);
  if (!ret) {
    return 0;
  }

  // Set up data for iterating through loaded shared objects.
  phdr_data pd;
  pd.backtrace = self;
  pd.on_error = on_error;
  pd.data = data;
  pd.on_get_file_line = &on_get_file_line_;
  pd.found_sym = &found_sym;
  pd.found_dwarf = &found_dwarf;
  pd.exe_filename = filename;
  pd.exe_descriptor = ret < 0 ? descriptor : -1;

  // Process all loaded shared objects.
  //
  // Note: If an ELF binary is dlopened after `dl_iterate_phdr` is called, it
  // will not be processed. So if we try to dump a backtrace from within such
  // an ELF binary, some symbols might not be dumped correctly. However, since
  // `ten_backtrace_init_posix` is only called at the time of the first
  // backtrace dump (i.e., it’s lazily initialized and not invoked right when
  // the executable starts), as long as all the ELF binaries that need to be
  // dlopened (e.g., TEN extensions) have already been loaded, then
  // `dl_iterate_phdr` will be able to process all the relevant ELF binaries.
  //
  // Since it's possible that multiple threads may perform a backtrace dump at
  // the same time, there could be multiple threads concurrently in the
  // backtrace initialization phase. Therefore, all fields that are passed
  // around and stored during this initialization phase for later use are
  // essentially handled using atomics to ensure thread safety.
  dl_iterate_phdr(phdr_callback, (void *)&pd);

  // Set up symbol lookup function based on whether we found symbol information.
  if (found_sym) {
    ten_atomic_ptr_store((ten_atomic_ptr_t *)&self_posix->on_get_syminfo,
                         elf_syminfo);
  } else {
    ten_atomic_ptr_store((ten_atomic_ptr_t *)&self_posix->on_get_syminfo,
                         elf_nosyms);
  }

  // Get the current file/line lookup function (if already set by another
  // thread).
  *on_get_file_line =
      (ten_backtrace_on_get_file_line_func_t)ten_atomic_ptr_load(
          (ten_atomic_ptr_t *)&self_posix->on_get_file_line);

  // If no file/line function is set yet, use the one we determined.
  if (*on_get_file_line == NULL || *on_get_file_line == elf_nodebug) {
    *on_get_file_line = on_get_file_line_;
  }

  return 1;
}
