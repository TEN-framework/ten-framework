//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/function.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/section.h"

typedef struct dwarf_data dwarf_data;
typedef struct unit_addrs unit_addrs;
typedef struct unit unit;
typedef struct unit_addrs_vector unit_addrs_vector;
typedef struct unit_vector unit_vector;
typedef struct function_vector function_vector;
typedef struct dwarf_sections dwarf_sections;

/**
 * @brief Main structure for DWARF debug information used to map program
 * counters to source locations.
 *
 * This structure serves as the central repository for all DWARF debug
 * information extracted from an executable or shared library. It maintains the
 * necessary data structures to efficiently map program counter (PC) values to
 * source file names and line numbers during stack trace generation.
 */
struct dwarf_data {
  // Pointer to the next dwarf_data structure in a linked list of debug
  // information.
  dwarf_data *next;

  // Pointer to debug information from a .gnu_debugaltlink section.
  // This contains supplementary debug information stored in a separate file.
  dwarf_data *altlink;

  // Base address where this file is loaded in memory.
  // Used to adjust addresses when mapping PC values to debug information.
  uintptr_t base_address;

  // Sorted array of address ranges mapped to compilation units.
  // This enables binary search to quickly find which unit contains a given PC.
  unit_addrs *addrs;

  // Number of entries in the addrs array.
  size_t addrs_count;

  // Array of pointers to compilation units parsed from the debug information.
  // Each unit represents a separately compiled source file.
  unit **units;

  // Number of compilation units in the units array.
  size_t units_count;

  // Raw DWARF section data extracted from the binary file.
  dwarf_sections dwarf_sections;

  // Endianness flag: non-zero if data is big-endian, zero if little-endian.
  // Determines how multi-byte values in the DWARF data are interpreted.
  int is_bigendian;

  // Vector for storing function address information.
  // Maintained at this level to allow for efficient growth as more
  // functions are discovered during parsing.
  function_vector fvec;
};
