//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdio.h>

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/abbreviation.h"
#include "include_internal/ten_utils/backtrace/vector.h"
#include "ten_utils/lib/atomic.h"

typedef struct function_addrs function_addrs;

/**
 * @brief DWARF 5 unit header types that identify the kind of compilation unit.
 *
 * These unit types are used in the unit_type field of DWARF 5 unit headers
 * to specify the purpose and structure of the debugging information unit.
 */
typedef enum dwarf_unit_type {
  DW_UT_compile = 0x01,        // Full compilation unit
  DW_UT_type = 0x02,           // Type unit
  DW_UT_partial = 0x03,        // Partial compilation unit
  DW_UT_skeleton = 0x04,       // Skeleton compilation unit
  DW_UT_split_compile = 0x05,  // Split full compilation unit
  DW_UT_split_type = 0x06,     // Split type unit
  DW_UT_lo_user = 0x80,        // Beginning of user-defined unit types
  DW_UT_hi_user = 0xff         // End of user-defined unit types
} dwarf_unit_type;

/**
 * @brief A DWARF compilation unit structure containing information needed to
 * map a PC to a file and line.
 *
 * This structure holds the essential data extracted from a DWARF compilation
 * unit, including unit boundaries, version information, and references to line
 * number and function information. It serves as the central data structure for
 * resolving program counter addresses to source locations.
 */
typedef struct unit {
  // Pointer to the first entry (DIE) for this compilation unit.
  const unsigned char *unit_data;
  // Length of the data for this compilation unit in bytes.
  size_t unit_data_len;
  // Offset of unit_data from the start of the information for this compilation
  // unit.
  size_t unit_data_offset;
  // Offset of the start of the compilation unit from the start of the
  // .debug_info section.
  size_t low_offset;
  // Offset of the end of the compilation unit from the start of the
  // .debug_info section.
  size_t high_offset;
  // DWARF version number (2-5) of this compilation unit.
  int version;
  // Boolean flag indicating whether this unit uses DWARF64 format (1) or
  // DWARF32 format (0).
  int is_dwarf64;
  // Size of addresses in bytes for the target architecture (typically 4 or 8).
  int addrsize;
  // Offset into the .debug_line section for this unit's line number program.
  off_t lineoff;
  // Base offset into the .debug_str_offsets section for this compilation unit
  // (DWARF5).
  uint64_t str_offsets_base;
  // Base offset into the .debug_addr section for this compilation unit
  // (DWARF5).
  uint64_t addr_base;
  // Base offset into the .debug_rnglists section for this compilation unit
  // (DWARF5).
  uint64_t rnglists_base;
  // Primary source file name for this compilation unit.
  const char *filename;
  // Compilation command working directory.
  const char *comp_dir;
  // Absolute file name path, only set if needed for resolution.
  const char *abs_filename;
  // The abbreviation table for this unit, used to decode DIE attributes.
  abbrevs abbrevs;

  // The fields above this point are read during initialization and may be
  // accessed freely. The fields below require synchronization as they may be
  // initialized by different threads.

  // PC to line number mapping array.
  // NULL if values have not been read yet.
  // (struct line *)-1 if there was an error reading the values.
  struct line *lines;
  // Atomic counter for the number of entries in the lines array.
  ten_atomic_t lines_count;

  // Array of function address ranges associated with this unit.
  function_addrs *function_addrs;
  // Atomic counter for the number of entries in the function_addrs array.
  ten_atomic_t function_addrs_count;
} unit;

/**
 * @brief An address range for a compilation unit. This maps a PC value to a
 * specific compilation unit.
 *
 * @note We invert the representation in DWARF: instead of listing the units and
 * attaching a list of ranges, we list the ranges and have each one point to the
 * unit. This lets us do a binary search to find the unit.
 */
typedef struct unit_addrs {
  // Range is LOW <= PC < HIGH. PC values within this range belong to this unit.
  uintptr_t low;
  uintptr_t high;
  // Pointer to the compilation unit for this address range.
  struct unit *u;
} unit_addrs;

/**
 * @brief A growable vector of compilation unit address ranges.
 *
 * This structure maintains a dynamic array of unit_addrs structures,
 * allowing for efficient storage and lookup of address ranges associated
 * with compilation units.
 */
typedef struct unit_addrs_vector {
  // Underlying vector storage containing struct unit_addrs elements.
  ten_vector_t vec;
  // Number of address ranges currently stored in the vector.
  size_t count;
} unit_addrs_vector;

/**
 * @brief A growable vector of compilation unit pointers.
 *
 * This structure maintains a dynamic array of pointers to unit structures,
 * allowing for efficient storage and management of compilation units.
 */
typedef struct unit_vector {
  // Underlying vector storage containing struct unit* elements.
  ten_vector_t vec;
  // Number of compilation unit pointers currently stored in the vector.
  size_t count;
} unit_vector;

TEN_UTILS_PRIVATE_API unit *find_unit(unit **pu, size_t units_count,
                                      size_t offset);
