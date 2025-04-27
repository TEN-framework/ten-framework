//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/form.h"
#include "include_internal/ten_utils/backtrace/vector.h"

typedef struct dwarf_data dwarf_data;
typedef struct unit unit;
typedef struct ten_backtrace_t ten_backtrace_t;

/**
 * @brief Standard DWARF line number opcodes used in the line number program.
 *
 * These opcodes control the line number state machine that generates the
 * line number table mapping addresses to source locations.
 */
typedef enum dwarf_line_number_op {
  DW_LNS_extended_op = 0x0,  // Extended opcode - followed by uleb128 length and
                             // extended opcode
  DW_LNS_copy = 0x1,         // Copy current state to the line number table
  DW_LNS_advance_pc = 0x2,   // Advance PC by a constant value
  DW_LNS_advance_line = 0x3,        // Advance line number by a signed value
  DW_LNS_set_file = 0x4,            // Set file register to a constant value
  DW_LNS_set_column = 0x5,          // Set column register to a constant value
  DW_LNS_negate_stmt = 0x6,         // Toggle the is_stmt register
  DW_LNS_set_basic_block = 0x7,     // Set the basic_block register to true
  DW_LNS_const_add_pc = 0x8,        // Add a constant value to the PC register
  DW_LNS_fixed_advance_pc = 0x9,    // Advance PC by a fixed-size constant
  DW_LNS_set_prologue_end = 0xa,    // Set the prologue_end register to true
  DW_LNS_set_epilogue_begin = 0xb,  // Set the epilogue_begin register to true
  DW_LNS_set_isa = 0xc,             // Set the ISA register to a constant value
} dwarf_line_number_op;

/**
 * @brief Extended DWARF line number opcodes used with DW_LNS_extended_op.
 *
 * These opcodes provide additional functionality beyond the standard opcodes
 * and are encoded differently in the line number program.
 */
typedef enum dwarf_extended_line_number_op {
  DW_LNE_end_sequence = 0x1,       // End of a sequence of addresses
  DW_LNE_set_address = 0x2,        // Set the address register to a given value
  DW_LNE_define_file = 0x3,        // Define a file name (DWARF 2-4)
  DW_LNE_set_discriminator = 0x4,  // Set the discriminator register (DWARF 4+)
} dwarf_extended_line_number_op;

/**
 * @brief Content type codes for the line number table's file_names entries.
 *
 * These codes are used in DWARF 5 to identify the type of content in the
 * directory and file name tables of the line number information.
 */
typedef enum dwarf_line_number_content_type {
  DW_LNCT_path = 0x1,             // Path or file name string
  DW_LNCT_directory_index = 0x2,  // Directory index (uleb128)
  DW_LNCT_timestamp = 0x3,        // Last modification timestamp (uleb128)
  DW_LNCT_size = 0x4,             // File size in bytes (uleb128)
  DW_LNCT_MD5 = 0x5,              // 16-byte MD5 checksum of the file
  DW_LNCT_lo_user = 0x2000,       // Beginning of user-defined content types
  DW_LNCT_hi_user = 0x3fff        // End of user-defined content types
} dwarf_line_number_content_type;

/**
 * @brief Structure representing a DWARF line number program header.
 *
 * This structure contains information from the line number program header,
 * which is used to interpret the line number program that maps addresses
 * to source code locations.
 */
typedef struct line_header {
  // Version of the line number information format.
  int version;
  // Size of addresses in bytes for the target architecture.
  int addrsize;
  // Minimum instruction length in bytes.
  unsigned int min_insn_len;
  // Maximum number of operations per instruction (DWARF 4+).
  unsigned int max_ops_per_insn;
  // Line base for special opcode calculations.
  int line_base;
  // Line range for special opcode calculations.
  unsigned int line_range;
  // Opcode base - the first special opcode value.
  unsigned int opcode_base;
  // Array of standard opcode lengths, indexed by (opcode - 1).
  const unsigned char *opcode_lengths;

  // Number of directory entries in the header.
  size_t dirs_count;
  // Array of directory path strings.
  const char **dirs;

  // Number of filename entries in the header.
  size_t filenames_count;
  // Array of filename strings.
  const char **filenames;
} line_header;

/**
 * @brief Structure describing a format entry in a DWARF 5 line header.
 *
 * In DWARF 5, the line header uses a more flexible format where the
 * content and encoding of directory and file entries are specified
 * by format descriptors.
 */
typedef struct line_header_format {
  // Line Number Content Type (LNCT) code defining what this entry represents.
  int lnct;
  // DWARF form code specifying how the entry's data is encoded.
  dwarf_form form;
} line_header_format;

/**
 * @brief Structure mapping a program counter (PC) value to a source file and
 * line.
 *
 * Each entry is valid for the range from its PC value up to (but not including)
 * the PC value of the next entry in a sorted array of these structures.
 */
typedef struct line {
  // Program counter (memory address) value.
  uintptr_t pc;
  // Source file name. Multiple entries often point to the same filename string.
  const char *filename;
  // Source line number.
  int lineno;
  // Original index in the unsorted array read from DWARF section.
  // Used to maintain stability during sorting operations.
  int idx;
} line;

/**
 * @brief A growable vector of line number information.
 *
 * This structure is used while reading line number information from DWARF
 * debug sections. It accumulates line entries that map program addresses
 * to source locations.
 */
typedef struct line_vector {
  // Vector storing the line entries (array of struct line).
  ten_vector_t vec;
  // Number of valid line mappings currently in the vector.
  size_t count;
} line_vector;

TEN_UTILS_PRIVATE_API int read_line_info(ten_backtrace_t *self,
                                         dwarf_data *ddata,
                                         ten_backtrace_on_error_func_t on_error,
                                         void *data, unit *u, line_header *hdr,
                                         line **lines, size_t *lines_count);

TEN_UTILS_PRIVATE_API void free_line_header(
    ten_backtrace_t *self, line_header *hdr,
    ten_backtrace_on_error_func_t on_error, void *data);
