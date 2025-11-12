//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

/**
 * @brief DWARF 5 range list entry types used in the .debug_rnglists section.
 *
 * These entry types define different ways to represent address ranges in
 * DWARF 5. Each entry type has a specific encoding format for start and end
 * addresses.
 */
typedef enum dwarf_range_list_entry {
  DW_RLE_end_of_list = 0x00,    // Marks the end of a range list
  DW_RLE_base_addressx = 0x01,  // Base address from .debug_addr section (index)
  DW_RLE_startx_endx =
      0x02,  // Start and end addresses as indices into .debug_addr
  DW_RLE_startx_length = 0x03,  // Start address index and length
  DW_RLE_offset_pair = 0x04,    // Offsets from base address
  DW_RLE_base_address = 0x05,   // New base address (absolute)
  DW_RLE_start_end = 0x06,      // Start and end addresses (absolute)
  DW_RLE_start_length = 0x07    // Start address and length
} dwarf_range_list_entry;
