//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

/**
 * @brief DWARF debugging information entry (DIE) tags.
 *
 * These tags identify the type of each debugging information entry.
 * This is a subset of the standard DWARF tags that are most relevant
 * for stack unwinding and backtrace generation.
 */
typedef enum dwarf_tag {
  DW_TAG_entry_point = 0x3,          // Entry point to a program or subprogram
  DW_TAG_compile_unit = 0x11,        // Compilation unit
  DW_TAG_inlined_subroutine = 0x1d,  // Inlined function call
  DW_TAG_subprogram = 0x2e,          // Function or method
  DW_TAG_skeleton_unit = 0x4a        // Skeleton compilation unit (DWARF 5)
} dwarf_tag;
