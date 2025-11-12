//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/platform/posix/dwarf_internal/attribute.h"

// This struct is used to gather address range information while reading
// attributes. We use this while building a mapping from address ranges to
// compilation units and then again while mapping from address ranges to
// function entries. Normally either lowpc/highpc is set or ranges is set.
typedef struct pcrange {
  uintptr_t lowpc;           // The low PC value.
  int have_lowpc;            // Whether a low PC value was found.
  int lowpc_is_addr_index;   // Whether lowpc is in .debug_addr.
  uintptr_t highpc;          // The high PC value.
  int have_highpc;           // Whether a high PC value was found.
  int highpc_is_relative;    // Whether highpc is relative to lowpc.
  int highpc_is_addr_index;  // Whether highpc is in .debug_addr.
  uint64_t ranges;           // Offset in ranges section.
  int have_ranges;           // Whether ranges is valid.
  int ranges_is_index;       // Whether ranges is DW_FORM_rnglistx.
} pcrange;

TEN_UTILS_PRIVATE_API void update_pcrange(const attr *attr, const attr_val *val,
                                          pcrange *pcrange);

TEN_UTILS_PRIVATE_API int add_ranges(
    ten_backtrace_t *self, const dwarf_sections *dwarf_sections,
    uintptr_t base_address, int is_bigendian, unit *u, uintptr_t base,
    const pcrange *pcrange,
    int (*add_range)(ten_backtrace_t *self, void *rdata, uintptr_t lowpc,
                     uintptr_t highpc, ten_backtrace_on_error_func_t on_error,
                     void *data, void *vec),
    void *rdata, ten_backtrace_on_error_func_t on_error, void *data, void *vec);
