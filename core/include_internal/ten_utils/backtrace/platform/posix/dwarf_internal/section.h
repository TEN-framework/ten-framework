//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

/**
 * @brief An enum for the DWARF debug sections used in stack unwinding and
 * debugging.
 *
 * These sections correspond to the standard DWARF debug sections in ELF files.
 */
typedef enum dwarf_section {
  DEBUG_INFO,         // .debug_info - Debugging information entries
  DEBUG_LINE,         // .debug_line - Line number information
  DEBUG_ABBREV,       // .debug_abbrev - Abbreviation tables
  DEBUG_RANGES,       // .debug_ranges - Address ranges
  DEBUG_STR,          // .debug_str - String table
  DEBUG_ADDR,         // .debug_addr - Address table (DWARF 5)
  DEBUG_STR_OFFSETS,  // .debug_str_offsets - String offsets (DWARF 5)
  DEBUG_LINE_STR,     // .debug_line_str - Line number strings (DWARF 5)
  DEBUG_RNGLISTS,     // .debug_rnglists - Range lists (DWARF 5)

  DEBUG_MAX  // Number of debug sections
} dwarf_section;

/**
 * @brief Container for DWARF debug section data.
 *
 * This structure holds pointers to the raw data of various DWARF debug sections
 * and their corresponding sizes. The sections are indexed using the
 * debug_section enum (which should be defined elsewhere in the codebase).
 *
 * Each debug section (like .debug_info, .debug_line, etc.) is stored as a
 * pointer to its raw data and its size in bytes. This allows the DWARF parser
 * to access all necessary debug information sections efficiently.
 */
typedef struct dwarf_sections {
  // Array of pointers to the raw data for each debug section.
  const unsigned char *data[DEBUG_MAX];

  // Array of sizes (in bytes) for each debug section.
  size_t size[DEBUG_MAX];
} dwarf_sections;
