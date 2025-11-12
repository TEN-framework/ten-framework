//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

/**
 * @brief DWARF form codes that describe the representation of attribute values.
 *
 * These form codes specify how attribute values are encoded in the DWARF
 * format. Each form indicates a specific data representation used for debugging
 * information.
 */
typedef enum dwarf_form {
  DW_FORM_addr = 0x01,          // Address (target-specific size)
  DW_FORM_block2 = 0x03,        // Block with 2-byte length prefix
  DW_FORM_block4 = 0x04,        // Block with 4-byte length prefix
  DW_FORM_data2 = 0x05,         // Constant: 2 bytes
  DW_FORM_data4 = 0x06,         // Constant: 4 bytes
  DW_FORM_data8 = 0x07,         // Constant: 8 bytes
  DW_FORM_string = 0x08,        // Null-terminated string
  DW_FORM_block = 0x09,         // Block with LEB128 length prefix
  DW_FORM_block1 = 0x0a,        // Block with 1-byte length prefix
  DW_FORM_data1 = 0x0b,         // Constant: 1 byte
  DW_FORM_flag = 0x0c,          // Flag: 1 byte (0 = false, non-0 = true)
  DW_FORM_sdata = 0x0d,         // Signed LEB128 number
  DW_FORM_strp = 0x0e,          // Offset to string in .debug_str section
  DW_FORM_udata = 0x0f,         // Unsigned LEB128 number
  DW_FORM_ref_addr = 0x10,      // Reference to DIE in another compilation unit
  DW_FORM_ref1 = 0x11,          // Reference within compilation unit: 1 byte
  DW_FORM_ref2 = 0x12,          // Reference within compilation unit: 2 bytes
  DW_FORM_ref4 = 0x13,          // Reference within compilation unit: 4 bytes
  DW_FORM_ref8 = 0x14,          // Reference within compilation unit: 8 bytes
  DW_FORM_ref_udata = 0x15,     // Reference within compilation unit: LEB128
  DW_FORM_indirect = 0x16,      // Form determined by initial value
  DW_FORM_sec_offset = 0x17,    // Offset into a DWARF section (DWARF 4)
  DW_FORM_exprloc = 0x18,       // DWARF expression/location description
  DW_FORM_flag_present = 0x19,  // Flag: presence implies true
  DW_FORM_ref_sig8 = 0x20,      // Type signature (8 bytes)

  // DWARF 5 forms
  DW_FORM_strx = 0x1a,      // String index
  DW_FORM_addrx = 0x1b,     // Address index
  DW_FORM_ref_sup4 = 0x1c,  // 4-byte offset to DIE in supplementary object file
  DW_FORM_strp_sup = 0x1d,  // Offset into supplementary string section
  DW_FORM_data16 = 0x1e,    // Constant: 16 bytes
  DW_FORM_line_strp = 0x1f,  // Offset into .debug_line_str section
  DW_FORM_implicit_const =
      0x21,                 // Value is implicit in abbreviation declaration
  DW_FORM_loclistx = 0x22,  // Index into .debug_loclists section
  DW_FORM_rnglistx = 0x23,  // Index into .debug_rnglists section
  DW_FORM_ref_sup8 = 0x24,  // 8-byte offset to DIE in supplementary object file
  DW_FORM_strx1 = 0x25,     // String index (1 byte)
  DW_FORM_strx2 = 0x26,     // String index (2 bytes)
  DW_FORM_strx3 = 0x27,     // String index (3 bytes)
  DW_FORM_strx4 = 0x28,     // String index (4 bytes)
  DW_FORM_addrx1 = 0x29,    // Address index (1 byte)
  DW_FORM_addrx2 = 0x2a,    // Address index (2 bytes)
  DW_FORM_addrx3 = 0x2b,    // Address index (3 bytes)
  DW_FORM_addrx4 = 0x2c,    // Address index (4 bytes)

  // GNU extensions
  DW_FORM_GNU_addr_index = 0x1f01,  // GNU address index
  DW_FORM_GNU_str_index = 0x1f02,   // GNU string index
  DW_FORM_GNU_ref_alt =
      0x1f20,  // Reference to DIE in alternate .debug_info section
  DW_FORM_GNU_strp_alt = 0x1f21  // Offset into alternate .debug_str section
} dwarf_form;
